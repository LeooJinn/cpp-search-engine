#include "search_engine/document_loader.h"
#include "search_engine/indexed_search.h"
#include "search_engine/linear_search.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

std::string join_query(int argc, char* argv[], int first_argument) {
    std::string query = argv[first_argument];
    for (int index = first_argument + 1; index < argc; ++index) {
        query += ' ';
        query += argv[index];
    }
    return query;
}

template <typename Engine>
int print_search_results(const Engine& engine,
                         const std::string& query,
                         const char* engine_name) {
    const auto results = engine.search(query);
    std::cout << "Engine: " << engine_name << '\n';
    std::cout << "Loaded " << engine.document_count() << " documents.\n";
    std::cout << "Results for: " << query << '\n';
    for (const auto& result : results) {
        std::cout << result.document_id << " (score " << result.score << ")\n";
    }
    return 0;
}

std::size_t parse_thread_count(const std::string& value) {
    std::size_t characters_read = 0;
    const auto thread_count = std::stoull(value, &characters_read);
    if (characters_read != value.size() || thread_count == 0) {
        throw std::invalid_argument("thread count must be a positive integer");
    }
    return static_cast<std::size_t>(thread_count);
}

int build_index(int argc, char* argv[]) {
    if (argc < 4 || argc > 6) {
        std::cerr << "Usage: search_cli --build-index <document-directory> <index-file> "
                     "[--threads <count>]\n";
        return 2;
    }

    std::size_t thread_count = 1;
    if (argc == 6) {
        if (std::string(argv[4]) != "--threads") {
            std::cerr << "Error: expected --threads <count>.\n";
            return 2;
        }
        thread_count = parse_thread_count(argv[5]);
    }

    auto documents = search_engine::load_directory(argv[2]);
    search_engine::IndexedSearchEngine engine;
    if (thread_count == 1) {
        for (auto& document : documents) {
            engine.add_document(std::move(document));
        }
    } else {
        engine.add_documents_parallel(std::move(documents), thread_count);
    }
    engine.save(argv[3]);

    std::cout << "Saved index: " << argv[3] << '\n'
              << "Documents: " << engine.document_count() << '\n'
              << "Terms: " << engine.indexed_term_count() << '\n';
    return 0;
}

int search_saved_index(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: search_cli --index <index-file> [--synonyms] [--fuzzy <distance>] <query>\n";
        return 2;
    }

    search_engine::SearchOptions options;
    int query_argument = 3;
    while (query_argument < argc &&
           std::string(argv[query_argument]).rfind("--", 0) == 0) {
        const std::string option = argv[query_argument];
        if (option == "--synonyms") {
            options.expand_synonyms = true;
            ++query_argument;
        } else if (option == "--fuzzy") {
            if (query_argument + 1 >= argc) {
                throw std::invalid_argument("--fuzzy requires a maximum edit distance");
            }
            options.allow_fuzzy = true;
            options.max_edit_distance = parse_thread_count(argv[query_argument + 1]);
            query_argument += 2;
        } else {
            throw std::invalid_argument("unknown search option: " + option);
        }
    }
    if (query_argument >= argc) {
        throw std::invalid_argument("a search query is required");
    }

    const auto engine = search_engine::IndexedSearchEngine::load(argv[2]);
    const auto query = join_query(argc, argv, query_argument);
    const auto results = engine.search(query, options);
    std::cout << "Engine: persistent inverted index\n"
              << "Loaded " << engine.document_count() << " documents.\n"
              << "Results for: " << query << '\n';
    for (const auto& result : results) {
        std::cout << result.document_id << " (score " << result.score << ")\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage:\n"
                     "  search_cli [--engine linear|indexed] <document-directory> <query>\n"
                     "  search_cli --build-index <document-directory> <index-file> [--threads <count>]\n"
                     "  search_cli --index <index-file> [--synonyms] [--fuzzy <distance>] <query>\n";
        return 2;
    }

    try {
        const std::string mode = argv[1];
        if (mode == "--build-index") {
            return build_index(argc, argv);
        }
        if (mode == "--index") {
            return search_saved_index(argc, argv);
        }

        bool use_indexed = false;
        int directory_argument = 1;
        int query_argument = 2;
        if (mode == "--engine") {
            if (argc < 5) {
                std::cerr << "Usage: search_cli --engine linear|indexed <document-directory> <query>\n";
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

        const auto query = join_query(argc, argv, query_argument);
        const auto documents = search_engine::load_directory(argv[directory_argument]);
        if (use_indexed) {
            search_engine::IndexedSearchEngine engine;
            for (const auto& document : documents) {
                engine.add_document(document);
            }
            return print_search_results(engine, query, "inverted index");
        }

        search_engine::LinearSearchEngine engine;
        for (const auto& document : documents) {
            engine.add_document(document);
        }
        return print_search_results(engine, query, "linear scan");
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
