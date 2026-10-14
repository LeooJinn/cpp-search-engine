#include "search_engine/document_loader.h"
#include "search_engine/indexed_search.h"
#include "search_engine/linear_search.h"

#include <iostream>
#include <exception>
#include <string>
#include <utility>

namespace {

template <typename Engine>
int run_search(const std::string& directory, const std::string& query, const char* engine_name) {
    Engine engine;
    for (auto& document : search_engine::load_directory(directory)) {
        engine.add_document(std::move(document));
    }

    const auto results = engine.search(query);
    std::cout << "Engine: " << engine_name << '\n';
    std::cout << "Loaded " << engine.document_count() << " documents.\n";
    std::cout << "Results for: " << query << '\n';
    for (const auto& result : results) {
        std::cout << result.document_id << " (score " << result.score << ")\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: search_cli [--engine linear|indexed] <document-directory> <query>\n";
        return 2;
    }

    bool use_indexed = false;
    int directory_argument = 1;
    int query_argument = 2;
    if (std::string(argv[1]) == "--engine") {
        if (argc < 5) {
            std::cerr << "Usage: search_cli [--engine linear|indexed] <document-directory> <query>\n";
            return 2;
        }

        const std::string engine_name = argv[2];
        if (engine_name != "linear" && engine_name != "indexed") {
            std::cerr << "Error: engine must be 'linear' or 'indexed'.\n";
            return 2;
        }
        use_indexed = engine_name == "indexed";
        directory_argument = 3;
        query_argument = 4;
    }

    std::string query = argv[query_argument];
    for (int index = query_argument + 1; index < argc; ++index) {
        query += ' ';
        query += argv[index];
    }

    try {
        if (use_indexed) {
            return run_search<search_engine::IndexedSearchEngine>(
                argv[directory_argument], query, "inverted index");
        }
        return run_search<search_engine::LinearSearchEngine>(
            argv[directory_argument], query, "linear scan");
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
