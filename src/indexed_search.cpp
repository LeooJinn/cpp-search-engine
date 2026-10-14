#include "search_engine/indexed_search.h"

#include "search_engine/synonyms.h"
#include "search_engine/tokenizer.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace search_engine {
namespace {

constexpr char kIndexMagic[] = "CPPSEARCHIDX";
constexpr std::uint64_t kIndexVersion = 1;
constexpr std::uint64_t kMaxSerializedString = 256ULL * 1024ULL * 1024ULL;

void write_u64(std::ofstream& output, std::uint64_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_string(std::ofstream& output, const std::string& value) {
    write_u64(output, value.size());
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::uint64_t read_u64(std::ifstream& input, const std::string& index_path) {
    std::uint64_t value = 0;
    input.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!input) {
        throw std::runtime_error("Corrupt index (truncated integer): " + index_path);
    }
    return value;
}

std::string read_string(std::ifstream& input, const std::string& index_path) {
    const auto length = read_u64(input, index_path);
    if (length > kMaxSerializedString || length > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Corrupt index (invalid string length): " + index_path);
    }

    std::string value(static_cast<std::size_t>(length), '\0');
    input.read(value.data(), static_cast<std::streamsize>(length));
    if (!input) {
        throw std::runtime_error("Corrupt index (truncated string): " + index_path);
    }
    return value;
}

std::size_t bounded_edit_distance(std::string_view left,
                                  std::string_view right,
                                  std::size_t max_distance) {
    const auto length_difference = left.size() > right.size()
                                       ? left.size() - right.size()
                                       : right.size() - left.size();
    if (length_difference > max_distance) {
        return max_distance + 1;
    }

    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t index = 0; index <= right.size(); ++index) {
        previous[index] = index;
    }

    for (std::size_t left_index = 1; left_index <= left.size(); ++left_index) {
        current[0] = left_index;
        std::size_t row_minimum = current[0];
        for (std::size_t right_index = 1; right_index <= right.size(); ++right_index) {
            const auto substitution_cost = left[left_index - 1] == right[right_index - 1] ? 0 : 1;
            current[right_index] = std::min({
                previous[right_index] + 1,
                current[right_index - 1] + 1,
                previous[right_index - 1] + substitution_cost,
            });
            row_minimum = std::min(row_minimum, current[right_index]);
        }
        if (row_minimum > max_distance) {
            return max_distance + 1;
        }
        std::swap(previous, current);
    }

    return previous[right.size()];
}

void sort_results(std::vector<SearchResult>& results) {
    std::sort(results.begin(), results.end(), [](const SearchResult& left, const SearchResult& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.document_id < right.document_id;
    });
}

}  // namespace

void IndexedSearchEngine::add_document(Document document) {
    const auto document_index = documents_.size();
    const auto terms = tokenize(document.text);
    std::unordered_map<std::string, std::size_t> term_counts;

    for (const auto& term : terms) {
        ++term_counts[term];
    }

    documents_.push_back(std::move(document));
    for (const auto& [term, count] : term_counts) {
        inverted_index_[term].push_back({document_index, count});
    }
}

void IndexedSearchEngine::add_documents_parallel(std::vector<Document> documents,
                                                  std::size_t thread_count) {
    if (documents.empty()) {
        return;
    }
    if (thread_count == 0) {
        throw std::invalid_argument("thread count must be greater than zero");
    }

    const std::size_t worker_count = std::min(thread_count, documents.size());
    std::vector<std::unordered_map<std::string, std::size_t>> document_terms(documents.size());
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (std::size_t worker = 0; worker < worker_count; ++worker) {
        const auto begin = (documents.size() * worker) / worker_count;
        const auto end = (documents.size() * (worker + 1)) / worker_count;
        workers.emplace_back([&documents, &document_terms, begin, end]() {
            for (std::size_t index = begin; index < end; ++index) {
                for (const auto& term : tokenize(documents[index].text)) {
                    ++document_terms[index][term];
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    const auto first_document_index = documents_.size();
    documents_.reserve(first_document_index + documents.size());
    for (auto& document : documents) {
        documents_.push_back(std::move(document));
    }

    for (std::size_t index = 0; index < document_terms.size(); ++index) {
        for (const auto& [term, count] : document_terms[index]) {
            inverted_index_[term].push_back({first_document_index + index, count});
        }
    }
}

std::vector<SearchResult> IndexedSearchEngine::search(std::string_view query) const {
    return search(query, SearchOptions{});
}

std::vector<SearchResult> IndexedSearchEngine::search(std::string_view query,
                                                      const SearchOptions& options) const {
    const auto query_terms = unique_tokens(query);
    if (query_terms.empty()) {
        return {};
    }

    if (!options.expand_synonyms && !options.allow_fuzzy) {
        std::vector<const std::vector<Posting>*> posting_lists;
        posting_lists.reserve(query_terms.size());
        for (const auto& term : query_terms) {
            const auto match = inverted_index_.find(term);
            if (match == inverted_index_.end()) {
                return {};
            }
            posting_lists.push_back(&match->second);
        }

        std::sort(posting_lists.begin(), posting_lists.end(), [](const auto* left, const auto* right) {
            return left->size() < right->size();
        });

        std::vector<SearchResult> results;
        results.reserve(posting_lists.front()->size());
        for (const auto& candidate : *posting_lists.front()) {
            bool contains_all_terms = true;
            std::size_t score = candidate.second;

            for (std::size_t list_index = 1; list_index < posting_lists.size(); ++list_index) {
                const auto& postings = *posting_lists[list_index];
                const auto match = std::lower_bound(
                    postings.begin(), postings.end(), candidate.first,
                    [](const Posting& posting, DocumentIndex document_index) {
                        return posting.first < document_index;
                    });

                if (match == postings.end() || match->first != candidate.first) {
                    contains_all_terms = false;
                    break;
                }
                score += match->second;
            }

            if (contains_all_terms) {
                results.push_back(SearchResult{documents_[candidate.first].id, score});
            }
        }
        sort_results(results);
        return results;
    }

    std::vector<std::vector<std::string>> term_groups;
    term_groups.reserve(query_terms.size());
    for (const auto& query_term : query_terms) {
        std::vector<std::string> candidates{query_term};
        std::unordered_set<std::string> seen{query_term};

        if (options.expand_synonyms) {
            for (const auto& synonym : synonym_terms(query_term)) {
                if (seen.insert(synonym).second) {
                    candidates.push_back(synonym);
                }
            }
        }

        if (options.allow_fuzzy) {
            for (const auto& [indexed_term, postings] : inverted_index_) {
                static_cast<void>(postings);
                if (bounded_edit_distance(query_term, indexed_term, options.max_edit_distance) <=
                    options.max_edit_distance &&
                    seen.insert(indexed_term).second) {
                    candidates.push_back(indexed_term);
                }
            }
        }
        term_groups.push_back(std::move(candidates));
    }

    std::unordered_map<DocumentIndex, std::size_t> scores;
    for (std::size_t group_index = 0; group_index < term_groups.size(); ++group_index) {
        std::unordered_map<DocumentIndex, std::size_t> group_scores;
        for (const auto& candidate_term : term_groups[group_index]) {
            const auto postings = inverted_index_.find(candidate_term);
            if (postings == inverted_index_.end()) {
                continue;
            }
            for (const auto& [document_index, frequency] : postings->second) {
                auto& score = group_scores[document_index];
                score = std::max(score, frequency);
            }
        }

        if (group_scores.empty()) {
            return {};
        }
        if (group_index == 0) {
            scores = std::move(group_scores);
            continue;
        }

        for (auto iterator = scores.begin(); iterator != scores.end();) {
            const auto match = group_scores.find(iterator->first);
            if (match == group_scores.end()) {
                iterator = scores.erase(iterator);
            } else {
                iterator->second += match->second;
                ++iterator;
            }
        }
        if (scores.empty()) {
            return {};
        }
    }

    std::vector<SearchResult> results;
    results.reserve(scores.size());
    for (const auto& [document_index, score] : scores) {
        results.push_back(SearchResult{documents_[document_index].id, score});
    }
    sort_results(results);
    return results;
}

std::size_t IndexedSearchEngine::document_count() const noexcept {
    return documents_.size();
}

std::size_t IndexedSearchEngine::indexed_term_count() const noexcept {
    return inverted_index_.size();
}

std::size_t IndexedSearchEngine::memory_estimate_bytes() const noexcept {
    std::size_t estimate = documents_.capacity() * sizeof(Document);
    for (const auto& document : documents_) {
        estimate += document.id.capacity();
        estimate += document.source.string().capacity();
        estimate += document.text.capacity();
    }
    estimate += inverted_index_.size() * (sizeof(std::string) + sizeof(std::vector<Posting>));
    for (const auto& [term, postings] : inverted_index_) {
        estimate += term.capacity();
        estimate += postings.capacity() * sizeof(Posting);
    }
    return estimate;
}

const Document* IndexedSearchEngine::find_document(std::string_view document_id) const noexcept {
    const auto match = std::find_if(documents_.begin(), documents_.end(), [document_id](const auto& document) {
        return document.id == document_id;
    });
    return match == documents_.end() ? nullptr : &*match;
}

const std::vector<Document>& IndexedSearchEngine::documents() const noexcept {
    return documents_;
}

void IndexedSearchEngine::save(const std::filesystem::path& index_path) const {
    std::ofstream output(index_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Unable to open index for writing: " + index_path.string());
    }

    output.write(kIndexMagic, sizeof(kIndexMagic) - 1);
    write_u64(output, kIndexVersion);
    write_u64(output, documents_.size());
    for (const auto& document : documents_) {
        write_string(output, document.id);
        write_string(output, document.source.string());
        write_string(output, document.text);
    }

    std::vector<std::string> terms;
    terms.reserve(inverted_index_.size());
    for (const auto& [term, postings] : inverted_index_) {
        static_cast<void>(postings);
        terms.push_back(term);
    }
    std::sort(terms.begin(), terms.end());

    write_u64(output, terms.size());
    for (const auto& term : terms) {
        write_string(output, term);
        const auto& postings = inverted_index_.at(term);
        write_u64(output, postings.size());
        for (const auto& [document_index, frequency] : postings) {
            write_u64(output, document_index);
            write_u64(output, frequency);
        }
    }

    if (!output) {
        throw std::runtime_error("Unable to finish writing index: " + index_path.string());
    }
}

IndexedSearchEngine IndexedSearchEngine::load(const std::filesystem::path& index_path) {
    std::ifstream input(index_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open index for reading: " + index_path.string());
    }

    const std::string path = index_path.string();
    std::string magic(sizeof(kIndexMagic) - 1, '\0');
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!input || magic != kIndexMagic) {
        throw std::runtime_error("Invalid index magic: " + path);
    }

    if (read_u64(input, path) != kIndexVersion) {
        throw std::runtime_error("Unsupported index version: " + path);
    }

    IndexedSearchEngine engine;
    const auto document_count = read_u64(input, path);
    if (document_count > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Corrupt index (invalid document count): " + path);
    }
    engine.documents_.reserve(static_cast<std::size_t>(document_count));
    for (std::uint64_t index = 0; index < document_count; ++index) {
        engine.documents_.push_back(Document{
            read_string(input, path),
            read_string(input, path),
            read_string(input, path),
        });
    }

    const auto term_count = read_u64(input, path);
    if (term_count > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Corrupt index (invalid term count): " + path);
    }
    for (std::uint64_t term_index = 0; term_index < term_count; ++term_index) {
        const auto term = read_string(input, path);
        const auto posting_count = read_u64(input, path);
        if (posting_count > std::numeric_limits<std::size_t>::max()) {
            throw std::runtime_error("Corrupt index (invalid posting count): " + path);
        }

        auto& postings = engine.inverted_index_[term];
        postings.reserve(static_cast<std::size_t>(posting_count));
        for (std::uint64_t posting_index = 0; posting_index < posting_count; ++posting_index) {
            const auto document_index = read_u64(input, path);
            const auto frequency = read_u64(input, path);
            if (document_index >= document_count || frequency == 0 ||
                document_index > std::numeric_limits<DocumentIndex>::max() ||
                frequency > std::numeric_limits<std::size_t>::max()) {
                throw std::runtime_error("Corrupt index (invalid posting): " + path);
            }
            postings.push_back({static_cast<DocumentIndex>(document_index),
                                static_cast<std::size_t>(frequency)});
        }
    }

    return engine;
}

}  // namespace search_engine
