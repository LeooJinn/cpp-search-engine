#pragma once

#include <cstddef>

namespace search_engine {

struct SearchOptions {
    bool expand_synonyms = false;
    bool allow_fuzzy = false;
    std::size_t max_edit_distance = 1;
};

}  // namespace search_engine
