#include "search_engine/document_loader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace search_engine {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open document: " + path.string());
    }

    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Unable to read document: " + path.string());
    }

    return contents.str();
}

}  // namespace

std::vector<Document> load_directory(const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error)) {
        throw std::invalid_argument("Document directory does not exist: " + directory.string());
    }
    if (error || !std::filesystem::is_directory(directory, error)) {
        throw std::invalid_argument("Document path is not a directory: " + directory.string());
    }

    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    std::vector<Document> documents;
    documents.reserve(paths.size());
    for (const auto& path : paths) {
        documents.push_back(Document{
            path.filename().string(),
            path,
            read_file(path),
        });
    }

    return documents;
}

}  // namespace search_engine
