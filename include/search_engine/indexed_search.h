#pragma once

#include "search_engine/search_result.h"
#include "search_engine/document.h"
#include "search_engine/query_options.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace search_engine {

class IndexedSearchEngine {
public:
    void add_document(Document document);

    void add_documents_parallel(std::vector<Document> documents, std::size_t thread_count);

    [[nodiscard]] std::vector<SearchResult> search(std::string_view query) const;

    [[nodiscard]] std::vector<SearchResult> search(std::string_view query,
                                                   const SearchOptions& options) const;

    [[nodiscard]] std::size_t document_count() const noexcept;

    [[nodiscard]] std::size_t indexed_term_count() const noexcept;

    void save(const std::filesystem::path& index_path) const;

    [[nodiscard]] static IndexedSearchEngine load(const std::filesystem::path& index_path);

private:
    using DocumentIndex = std::size_t;
    using Posting = std::pair<DocumentIndex, std::size_t>;

    std::vector<Document> documents_;
    std::unordered_map<std::string, std::vector<Posting>> inverted_index_;
};

}  // namespace search_engine
