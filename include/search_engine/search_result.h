#pragma once

#include <cstddef>
#include <string>

namespace search_engine {

struct SearchResult {
    std::string document_id;
    std::size_t score;
};

}  // namespace search_engine
