#include "search_engine/indexed_search.h"
#include "search_engine/pdf_loader.h"
#include "search_engine/sqlite_loader.h"
#include "search_engine/web_loader.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void save_documents(std::vector<search_engine::Document> documents,
                    const std::string& index_path,
                    std::size_t thread_count) {
    search_engine::IndexedSearchEngine engine;
    if (thread_count == 1) {
        for (auto& document : documents) {
            engine.add_document(std::move(document));
        }
    } else {
        engine.add_documents_parallel(std::move(documents), thread_count);
    }
    engine.save(index_path);
    std::cout << "Saved index: " << index_path << '\n'
              << "Documents: " << engine.document_count() << '\n'
              << "Terms: " << engine.indexed_term_count() << '\n';
}

std::size_t parse_threads(int argc, char* argv[], int argument) {
    if (argc <= argument) {
        return 1;
    }
    if (std::string(argv[argument]) != "--threads" || argc != argument + 2) {
        throw std::invalid_argument("expected --threads <positive-count>");
    }
    const auto value = std::stoull(argv[argument + 1]);
    if (value == 0) {
        throw std::invalid_argument("thread count must be greater than zero");
    }
    return static_cast<std::size_t>(value);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage:\n"
                     "  search_ingest pdf <file.pdf> <index-file> [--threads <count>]\n"
                     "  search_ingest web <url> <index-file>\n"
                     "  search_ingest sqlite <database.db> <query> <index-file> [--threads <count>]\n";
        return 2;
    }

    try {
        const std::string source_type = argv[1];
        if (source_type == "pdf") {
            const auto threads = parse_threads(argc, argv, 4);
            save_documents(search_engine::load_pdf(argv[2]), argv[3], threads);
            return 0;
        }
        if (source_type == "web") {
            if (argc != 4) {
                throw std::invalid_argument("web ingestion does not accept extra arguments");
            }
            save_documents(search_engine::load_web_page(argv[2]), argv[3], 1);
            return 0;
        }
        if (source_type == "sqlite") {
            const auto threads = parse_threads(argc, argv, 5);
            save_documents(search_engine::load_sqlite(argv[2], argv[3]), argv[4], threads);
            return 0;
        }

        throw std::invalid_argument("source must be pdf, web, or sqlite");
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
