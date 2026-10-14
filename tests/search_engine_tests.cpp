#include "search_engine/document_loader.h"
#include "search_engine/indexed_search.h"
#include "search_engine/linear_search.h"
#include "search_engine/tokenizer.h"

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
        test_document_loader_reads_sorted_regular_files();
        test_document_loader_rejects_missing_directory();
        std::cout << "All search-engine tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
