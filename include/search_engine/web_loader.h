#pragma once

#include "search_engine/document.h"

#include <string_view>
#include <vector>

namespace search_engine {

std::vector<Document> load_web_page(std::string_view url);

}  // namespace search_engine
