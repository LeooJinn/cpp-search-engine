#pragma once

#include "search_engine/document.h"

#include <filesystem>
#include <vector>

namespace search_engine {

std::vector<Document> load_directory(const std::filesystem::path& directory);

}  // namespace search_engine
