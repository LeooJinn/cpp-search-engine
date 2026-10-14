#pragma once

#include "search_engine/document.h"

#include <filesystem>
#include <vector>

namespace search_engine {

std::vector<Document> load_pdf(const std::filesystem::path& pdf_path);

}  // namespace search_engine
