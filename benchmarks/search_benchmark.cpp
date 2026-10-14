#include "search_engine/document.h"
#include "search_engine/indexed_search.h"
#include "search_engine/linear_search.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::vector<search_engine::Document> make_corpus(std::size_t document_count) {
    std::vector<search_engine::Document> documents;
    documents.reserve(document_count);

    for (std::size_t index = 0; index < document_count; ++index) {
        std::string text = "document " + std::to_string(index) + " contains common data ";
        for (int repetition = 0; repetition < 80; ++repetition) {
            text += "systems programming algorithms testing ";
        }
        if (index % 3 == 0) {
            text += "search ";
        }
        if (index % 5 == 0) {
            text += "engine ";
        }

        documents.push_back(search_engine::Document{
            "document-" + std::to_string(index),
            {},
            std::move(text),
        });
    }

    return documents;
}

template <typename Engine>
std::pair<double, Engine> build_engine(std::vector<search_engine::Document> documents) {
    Engine engine;
    const auto start = Clock::now();
    for (auto& document : documents) {
        engine.add_document(std::move(document));
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return {elapsed, std::move(engine)};
}

std::pair<double, search_engine::IndexedSearchEngine> build_parallel_engine(
    std::vector<search_engine::Document> documents, std::size_t thread_count) {
    search_engine::IndexedSearchEngine engine;
    const auto start = Clock::now();
    engine.add_documents_parallel(std::move(documents), thread_count);
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return {elapsed, std::move(engine)};
}

template <typename Engine>
std::pair<double, std::size_t> benchmark_queries(const Engine& engine,
                                                  std::string_view query,
                                                  std::size_t repetitions) {
    std::size_t total_matches = 0;
    const auto start = Clock::now();
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        total_matches += engine.search(query).size();
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return {elapsed, total_matches};
}

}  // namespace

int main() {
    constexpr std::size_t document_count = 2'000;
    constexpr std::size_t query_repetitions = 25;
    constexpr std::size_t parallel_threads = 4;
    constexpr std::string_view query = "search engine";

    auto [linear_build_ms, linear] = build_engine<search_engine::LinearSearchEngine>(
        make_corpus(document_count));
    auto [indexed_build_ms, indexed] = build_engine<search_engine::IndexedSearchEngine>(
        make_corpus(document_count));
    auto [parallel_build_ms, parallel] = build_parallel_engine(
        make_corpus(document_count), parallel_threads);

    const auto [linear_query_ms, linear_matches] =
        benchmark_queries(linear, query, query_repetitions);
    const auto [indexed_query_ms, indexed_matches] =
        benchmark_queries(indexed, query, query_repetitions);
    const auto [parallel_query_ms, parallel_matches] =
        benchmark_queries(parallel, query, query_repetitions);

    std::cout << "C++ search engine benchmark\n"
              << "documents=" << document_count << '\n'
              << "query_repetitions=" << query_repetitions << '\n'
              << "parallel_threads=" << parallel_threads << '\n'
              << "query=\"" << query << "\"\n"
              << "linear_build_ms=" << linear_build_ms << '\n'
              << "indexed_build_ms=" << indexed_build_ms << '\n'
              << "parallel_indexed_build_ms=" << parallel_build_ms << '\n'
              << "linear_query_ms=" << linear_query_ms << '\n'
              << "indexed_query_ms=" << indexed_query_ms << '\n'
              << "parallel_indexed_query_ms=" << parallel_query_ms << '\n'
              << "linear_total_matches=" << linear_matches << '\n'
              << "indexed_total_matches=" << indexed_matches << '\n'
              << "parallel_indexed_total_matches=" << parallel_matches << '\n';

    return linear_matches == indexed_matches && indexed_matches == parallel_matches ? 0 : 1;
}
