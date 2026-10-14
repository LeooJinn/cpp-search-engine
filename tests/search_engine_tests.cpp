#include "search_engine/document_loader.h"
#include "search_engine/indexed_search.h"
#include "search_engine/linear_search.h"
#include "search_engine/pdf_loader.h"
#include "search_engine/sqlite_loader.h"
#include "search_engine/tokenizer.h"
#include "search_engine/web_loader.h"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("FAIL: " + message);
    }
}

void test_tokenizer_normalizes_text() {
    const auto tokens = search_engine::tokenize("Hello, WORLD! C++17 data-driven.");
    require(tokens == std::vector<std::string>{"hello", "world", "c", "17", "data", "driven"},
            "tokenizer should lowercase words and split punctuation");
}

void test_tokenizer_handles_empty_text() {
    require(search_engine::tokenize("... \t\n") .empty(), "tokenizer should ignore text without tokens");
}

void test_linear_search_requires_all_terms_and_ranks_matches() {
    search_engine::LinearSearchEngine engine;
    engine.add_document({"first", {}, "alpha alpha beta"});
    engine.add_document({"second", {}, "alpha beta beta beta"});
    engine.add_document({"partial", {}, "alpha only"});

    const auto results = engine.search("ALPHA beta");
    require(results.size() == 2, "search should return only documents containing every query term");
    require(results[0].document_id == "second" && results[0].score == 4,
            "search should rank by matching term frequency");
    require(results[1].document_id == "first" && results[1].score == 3,
            "search should return the next matching document");
}

void test_linear_search_deduplicates_query_terms() {
    search_engine::LinearSearchEngine engine;
    engine.add_document({"doc", {}, "alpha alpha"});

    const auto results = engine.search("alpha alpha");
    require(results.size() == 1 && results[0].score == 2,
            "repeating a query term should not change the required term set");
    require(engine.search("   ").empty(), "an empty query should return no results");
}

void test_indexed_search_matches_linear_search() {
    const std::vector<search_engine::Document> documents{
        {"first", {}, "alpha alpha beta"},
        {"second", {}, "alpha beta beta beta"},
        {"partial", {}, "alpha only"},
    };
    search_engine::LinearSearchEngine linear;
    search_engine::IndexedSearchEngine indexed;
    for (const auto& document : documents) {
        linear.add_document(document);
        indexed.add_document(document);
    }

    const auto linear_results = linear.search("ALPHA beta");
    const auto indexed_results = indexed.search("ALPHA beta");
    require(indexed_results.size() == linear_results.size(),
            "indexed search should return the same number of results as the linear baseline");
    for (std::size_t index = 0; index < linear_results.size(); ++index) {
        require(indexed_results[index].document_id == linear_results[index].document_id &&
                    indexed_results[index].score == linear_results[index].score,
                "indexed search should produce the same results as the linear baseline");
    }
    require(indexed.document_count() == documents.size(),
            "indexed search should track the number of documents");
    require(indexed.indexed_term_count() == 3,
            "indexed search should track unique terms in the corpus");
    require(indexed.search("missing").empty(),
            "indexed search should return no results for an unknown term");
    require(indexed.search("   ").empty(),
            "indexed search should return no results for an empty query");
}

void test_parallel_indexing_matches_sequential_indexing() {
    const std::vector<search_engine::Document> documents{
        {"first", {}, "alpha alpha beta"},
        {"second", {}, "alpha beta beta beta"},
        {"partial", {}, "alpha only"},
        {"fourth", {}, "beta gamma"},
    };
    search_engine::IndexedSearchEngine sequential;
    search_engine::IndexedSearchEngine parallel;
    for (const auto& document : documents) {
        sequential.add_document(document);
    }
    parallel.add_documents_parallel(documents, 2);

    const auto sequential_results = sequential.search("alpha beta");
    const auto parallel_results = parallel.search("alpha beta");
    require(parallel_results.size() == sequential_results.size(),
            "parallel indexing should return the same number of results");
    for (std::size_t index = 0; index < sequential_results.size(); ++index) {
        require(parallel_results[index].document_id == sequential_results[index].document_id &&
                    parallel_results[index].score == sequential_results[index].score,
                "parallel indexing should preserve sequential search results");
    }
    require(parallel.document_count() == documents.size(),
            "parallel indexing should track every document");

    bool threw = false;
    try {
        search_engine::IndexedSearchEngine empty;
        empty.add_documents_parallel(documents, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "parallel indexing should reject zero worker threads");
}

void test_query_expansion_supports_synonyms_and_typos() {
    search_engine::IndexedSearchEngine engine;
    engine.add_document({"vehicle", {}, "automobile repair"});
    engine.add_document({"algorithm", {}, "algorithm design"});

    require(engine.search("car").empty(),
            "exact search should not invent synonym matches");

    search_engine::SearchOptions synonym_options;
    synonym_options.expand_synonyms = true;
    const auto synonym_results = engine.search("car", synonym_options);
    require(synonym_results.size() == 1 && synonym_results[0].document_id == "vehicle",
            "synonym expansion should match configured equivalent terms");

    search_engine::SearchOptions fuzzy_options;
    fuzzy_options.allow_fuzzy = true;
    const auto fuzzy_results = engine.search("algoritm", fuzzy_options);
    require(fuzzy_results.size() == 1 && fuzzy_results[0].document_id == "algorithm",
            "fuzzy search should match a one-edit spelling error");
}

void test_persistent_index_round_trip() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto index_path = std::filesystem::temp_directory_path() /
                            ("cpp-search-engine-index-" + std::to_string(suffix) + ".bin");
    search_engine::IndexedSearchEngine original;
    original.add_document({"first", "source-a.txt", "alpha alpha beta"});
    original.add_document({"second", "source-b.txt", "alpha beta beta beta"});

    original.save(index_path);
    const auto loaded = search_engine::IndexedSearchEngine::load(index_path);
    const auto original_results = original.search("alpha beta");
    const auto loaded_results = loaded.search("alpha beta");

    require(loaded.document_count() == original.document_count(),
            "loaded index should preserve document count");
    require(loaded.indexed_term_count() == original.indexed_term_count(),
            "loaded index should preserve term count");
    require(loaded_results.size() == original_results.size(),
            "loaded index should preserve result count");
    for (std::size_t index = 0; index < original_results.size(); ++index) {
        require(loaded_results[index].document_id == original_results[index].document_id &&
                    loaded_results[index].score == original_results[index].score,
                "loaded index should preserve search results");
    }

    std::filesystem::remove(index_path);
}

void test_corrupt_persistent_index_is_rejected() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto index_path = std::filesystem::temp_directory_path() /
                            ("cpp-search-engine-corrupt-" + std::to_string(suffix) + ".bin");
    {
        std::ofstream output(index_path, std::ios::binary);
        output << "not an index";
    }

    bool threw = false;
    try {
        static_cast<void>(search_engine::IndexedSearchEngine::load(index_path));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::filesystem::remove(index_path);
    require(threw, "corrupt indexes should be rejected");
}

void test_sqlite_loader_reads_document_rows() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto database_path = std::filesystem::temp_directory_path() /
                                ("cpp-search-engine-data-" + std::to_string(suffix) + ".db");
    sqlite3* database = nullptr;
    require(sqlite3_open(database_path.string().c_str(), &database) == SQLITE_OK,
            "test SQLite database should open");
    const char* setup =
        "CREATE TABLE documents (id TEXT NOT NULL, body TEXT NOT NULL);"
        "INSERT INTO documents VALUES ('one', 'alpha beta');"
        "INSERT INTO documents VALUES ('two', 'beta gamma');";
    char* error_message = nullptr;
    require(sqlite3_exec(database, setup, nullptr, nullptr, &error_message) == SQLITE_OK,
            "test SQLite database should be populated");
    sqlite3_free(error_message);
    sqlite3_close(database);

    const auto documents = search_engine::load_sqlite(
        database_path, "SELECT id, body FROM documents ORDER BY id");
    require(documents.size() == 2, "SQLite loader should return each selected row");
    require(documents[0].id == "one" && documents[0].text == "alpha beta",
            "SQLite loader should map the first two columns to document ID and text");
    require(documents[1].id == "two" && documents[1].text == "beta gamma",
            "SQLite loader should preserve row order from the query");

    std::filesystem::remove(database_path);
}

void test_external_loaders_validate_inputs() {
    bool pdf_threw = false;
    try {
        static_cast<void>(search_engine::load_pdf("/path/that/does/not/exist.pdf"));
    } catch (const std::invalid_argument&) {
        pdf_threw = true;
    }
    require(pdf_threw, "PDF loader should reject a missing file");

    bool web_threw = false;
    try {
        static_cast<void>(search_engine::load_web_page("file:///etc/passwd"));
    } catch (const std::invalid_argument&) {
        web_threw = true;
    }
    require(web_threw, "web loader should reject non-HTTP schemes");
}

void test_document_loader_reads_sorted_regular_files() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() /
                           ("cpp-search-engine-test-" + std::to_string(suffix));
    std::filesystem::create_directories(directory);

    {
        std::ofstream(directory / "b.txt") << "second file";
        std::ofstream(directory / "a.txt") << "first file";
        std::filesystem::create_directory(directory / "nested");
    }

    const auto documents = search_engine::load_directory(directory);
    require(documents.size() == 2, "loader should load regular files and skip directories");
    require(documents[0].id == "a.txt" && documents[1].id == "b.txt",
            "loader should return files in deterministic path order");
    require(documents[0].text == "first file", "loader should preserve file contents");

    std::filesystem::remove_all(directory);
}

void test_document_loader_rejects_missing_directory() {
    bool threw = false;
    try {
        search_engine::load_directory("/path/that/does/not/exist");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "loader should reject a missing directory");
}

}  // namespace

int main() {
    try {
        test_tokenizer_normalizes_text();
        test_tokenizer_handles_empty_text();
        test_linear_search_requires_all_terms_and_ranks_matches();
        test_linear_search_deduplicates_query_terms();
        test_indexed_search_matches_linear_search();
        test_parallel_indexing_matches_sequential_indexing();
        test_query_expansion_supports_synonyms_and_typos();
        test_persistent_index_round_trip();
        test_corrupt_persistent_index_is_rejected();
        test_sqlite_loader_reads_document_rows();
        test_external_loaders_validate_inputs();
        test_document_loader_reads_sorted_regular_files();
        test_document_loader_rejects_missing_directory();
        std::cout << "All search-engine tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
