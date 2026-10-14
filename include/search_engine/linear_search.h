#pragma once

#include "search_engine/document.h"
#include "search_engine/search_result.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace search_engine {

class LinearSearchEngine {
public:
    void add_document(Document document);

    [[nodiscard]] std::vector<SearchResult> search(std::string_view query) const;

    [[nodiscard]] std::size_t document_count() const noexcept;

    [[nodiscard]] std::size_t memory_estimate_bytes() const noexcept;

private:
    std::vector<Document> documents_;
};

}  // namespace search_engine
