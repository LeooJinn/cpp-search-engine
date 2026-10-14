#include "search_engine/indexed_search.h"

#include "search_engine/tokenizer.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace search_engine {

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
    const auto query_terms = unique_tokens(query);
    if (query_terms.empty()) {
        return {};
    }

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

    std::sort(results.begin(), results.end(), [](const SearchResult& left, const SearchResult& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.document_id < right.document_id;
    });

    return results;
}

std::size_t IndexedSearchEngine::document_count() const noexcept {
    return documents_.size();
}

std::size_t IndexedSearchEngine::indexed_term_count() const noexcept {
    return inverted_index_.size();
}

}  // namespace search_engine
