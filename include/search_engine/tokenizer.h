#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace search_engine {

std::vector<std::string> tokenize(std::string_view text);

std::vector<std::string> unique_tokens(std::string_view text);

}  // namespace search_engine
