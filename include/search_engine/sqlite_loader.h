#pragma once

#include "search_engine/document.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace search_engine {

// The query must return at least two columns: document ID and document text.
std::vector<Document> load_sqlite(const std::filesystem::path& database_path,
                                  std::string_view query);

}  // namespace search_engine
