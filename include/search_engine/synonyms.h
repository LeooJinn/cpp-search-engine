#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace search_engine {

std::vector<std::string> synonym_terms(std::string_view term);

}  // namespace search_engine
