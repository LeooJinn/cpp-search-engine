#include "search_engine/linear_search.h"

#include "search_engine/tokenizer.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace search_engine {

void LinearSearchEngine::add_document(Document document) {
    documents_.push_back(std::move(document));
}

std::vector<SearchResult> LinearSearchEngine::search(std::string_view query) const {
    const auto query_terms = unique_tokens(query);
    if (query_terms.empty()) {
        return {};
    }

    std::vector<SearchResult> results;
    for (const auto& document : documents_) {
        const auto document_terms = tokenize(document.text);
        std::unordered_map<std::string, std::size_t> term_counts;
        for (const auto& term : document_terms) {
            ++term_counts[term];
        }

        bool contains_all_terms = true;
        std::size_t score = 0;
        for (const auto& term : query_terms) {
            const auto match = term_counts.find(term);
            if (match == term_counts.end()) {
                contains_all_terms = false;
                break;
            }
            score += match->second;
        }

        if (contains_all_terms) {
            results.push_back(SearchResult{document.id, score});
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

std::size_t LinearSearchEngine::document_count() const noexcept {
    return documents_.size();
}

}  // namespace search_engine
