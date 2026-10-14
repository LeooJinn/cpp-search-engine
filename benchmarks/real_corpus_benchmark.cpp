#include "search_engine/document_loader.h"
#include "search_engine/indexed_search.h"
#include "search_engine/tokenizer.h"

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct TimingStats {
    std::vector<double> samples;

    [[nodiscard]] double percentile(double fraction) const {
        if (samples.empty()) return 0.0;
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const auto index = static_cast<std::size_t>(fraction * (sorted.size() - 1));
        return sorted[index];
    }

    [[nodiscard]] double mean() const {
        if (samples.empty()) return 0.0;
        return std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    }
};

struct Config {
    std::filesystem::path corpus;
    std::filesystem::path queries;
    std::filesystem::path output;
    std::filesystem::path index_output;
    std::size_t threads = 4;
    std::size_t runs = 3;
    std::size_t query_repetitions = 3;
};

struct Metrics {
    std::size_t documents = 0;
    std::size_t corpus_bytes = 0;
    std::size_t token_estimate = 0;
    std::size_t unique_terms = 0;
    TimingStats sequential_build;
    TimingStats parallel_build;
    TimingStats save;
    TimingStats load;
    TimingStats query;
    std::size_t index_bytes = 0;
    std::size_t memory_estimate_bytes = 0;
    std::size_t peak_rss_bytes = 0;
    std::size_t query_matches = 0;
};

std::size_t peak_rss_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#ifdef __APPLE__
    return static_cast<std::size_t>(usage.ru_maxrss);
#else
    return static_cast<std::size_t>(usage.ru_maxrss) * 1024ULL;
#endif
}

std::size_t parse_positive(std::string_view flag, std::string_view value) {
    std::size_t consumed = 0;
    const auto number = std::stoull(std::string(value), &consumed);
    if (consumed != value.size() || number == 0) {
        throw std::invalid_argument(std::string(flag) + " must be a positive integer");
    }
    return static_cast<std::size_t>(number);
}

Config parse_arguments(int argc, char* argv[]) {
    Config config;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        auto require_value = [&](const char* flag) -> std::string_view {
            if (index + 1 >= argc) {
                throw std::invalid_argument(std::string(flag) + " requires a value");
            }
            return argv[++index];
        };

        if (argument == "--corpus") {
            config.corpus = require_value("--corpus");
        } else if (argument == "--queries") {
            config.queries = require_value("--queries");
        } else if (argument == "--output") {
            config.output = require_value("--output");
        } else if (argument == "--index-output") {
            config.index_output = require_value("--index-output");
        } else if (argument == "--threads") {
            config.threads = parse_positive("--threads", require_value("--threads"));
        } else if (argument == "--runs") {
            config.runs = parse_positive("--runs", require_value("--runs"));
        } else if (argument == "--query-repetitions") {
            config.query_repetitions = parse_positive(
                "--query-repetitions", require_value("--query-repetitions"));
        } else if (argument == "--help") {
            std::cout << "Usage: search_real_benchmark --corpus <dir> --queries <file>\n"
                         "  [--output <markdown>] [--index-output <file>]\n"
                         "  [--threads 4] [--runs 3] [--query-repetitions 3]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }

    if (config.corpus.empty() || config.queries.empty()) {
        throw std::invalid_argument("--corpus and --queries are required");
    }
    return config;
}

std::vector<std::string> read_queries(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Unable to open query file: " + path.string());

    std::vector<std::string> queries;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.front() != '#') queries.push_back(std::move(line));
    }
    if (queries.empty()) throw std::invalid_argument("query file contains no queries");
    return queries;
}

std::vector<search_engine::Document> copy_documents(
    const std::vector<search_engine::Document>& documents) {
    return documents;
}

std::pair<double, search_engine::IndexedSearchEngine> build_sequential(
    const std::vector<search_engine::Document>& documents) {
    search_engine::IndexedSearchEngine engine;
    const auto start = Clock::now();
    for (auto document : copy_documents(documents)) engine.add_document(std::move(document));
    return {std::chrono::duration<double, std::milli>(Clock::now() - start).count(), std::move(engine)};
}

std::pair<double, search_engine::IndexedSearchEngine> build_parallel(
    const std::vector<search_engine::Document>& documents, std::size_t threads) {
    search_engine::IndexedSearchEngine engine;
    const auto start = Clock::now();
    engine.add_documents_parallel(copy_documents(documents), threads);
    return {std::chrono::duration<double, std::milli>(Clock::now() - start).count(), std::move(engine)};
}

std::pair<double, search_engine::IndexedSearchEngine> load_index(
    const std::filesystem::path& path) {
    const auto start = Clock::now();
    auto engine = search_engine::IndexedSearchEngine::load(path);
    return {std::chrono::duration<double, std::milli>(Clock::now() - start).count(), std::move(engine)};
}

void write_report(std::ostream& output, const Config& config, const Metrics& metrics,
                  const std::vector<std::string>& queries) {
    output << "# Real Corpus Search Benchmark\n\n"
           << "- Corpus: `" << config.corpus.string() << "`\n"
           << "- Queries: `" << config.queries.string() << "`\n"
           << "- Documents: " << metrics.documents << "\n"
           << "- Corpus bytes: " << metrics.corpus_bytes << "\n"
           << "- Query count: " << queries.size() << "\n"
           << "- Token count: " << metrics.token_estimate << "\n"
           << "- Unique terms: " << metrics.unique_terms << "\n"
           << "- Build runs: " << config.runs << "\n"
           << "- Query repetitions per run: " << config.query_repetitions << "\n"
           << "- Parallel workers: " << config.threads << "\n\n"
           << "The corpus is loaded through the production `load_directory()` path. "
              "Memory is an in-process estimate plus the peak RSS observed by `getrusage`; it is not a full allocator profile.\n\n"
           << "| Metric | Mean / value |\n|---|---:|\n"
           << std::fixed << std::setprecision(3)
           << "| Sequential build (ms) | " << metrics.sequential_build.mean() << " |\n"
           << "| Parallel build (ms) | " << metrics.parallel_build.mean() << " |\n"
           << "| Index save (ms) | " << metrics.save.mean() << " |\n"
           << "| Index load (ms) | " << metrics.load.mean() << " |\n"
           << "| Query p50 (ms) | " << metrics.query.percentile(.50) << " |\n"
           << "| Query p95 (ms) | " << metrics.query.percentile(.95) << " |\n"
           << "| Query p99 (ms) | " << metrics.query.percentile(.99) << " |\n"
           << "| Index bytes | " << metrics.index_bytes << " |\n"
           << "| Estimated indexed memory | " << metrics.memory_estimate_bytes << " |\n"
           << "| Peak RSS | " << metrics.peak_rss_bytes << " |\n"
           << "| Total query matches | " << metrics.query_matches << " |\n\n"
           << "## Query set\n\n";
    for (const auto& query : queries) output << "- `" << query << "`\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto config = parse_arguments(argc, argv);
        const auto documents = search_engine::load_directory(config.corpus);
        const auto queries = read_queries(config.queries);
        if (documents.empty()) throw std::invalid_argument("corpus directory contains no files");

        Metrics metrics;
        metrics.documents = documents.size();
        for (const auto& document : documents) {
            metrics.corpus_bytes += document.text.size();
            metrics.token_estimate += search_engine::tokenize(document.text).size();
        }

        for (std::size_t run = 0; run < config.runs; ++run) {
            auto [sequential_time, sequential] = build_sequential(documents);
            auto [parallel_time, parallel] = build_parallel(documents, config.threads);
            metrics.sequential_build.samples.push_back(sequential_time);
            metrics.parallel_build.samples.push_back(parallel_time);
            metrics.unique_terms = parallel.indexed_term_count();

            const auto index_path = config.index_output.empty()
                                        ? std::filesystem::temp_directory_path() / "cpp-search-real.index"
                                        : config.index_output;
            const auto save_start = Clock::now();
            parallel.save(index_path);
            metrics.save.samples.push_back(
                std::chrono::duration<double, std::milli>(Clock::now() - save_start).count());
            metrics.index_bytes = std::filesystem::file_size(index_path);

            auto [load_time, loaded] = load_index(index_path);
            metrics.load.samples.push_back(load_time);
            metrics.memory_estimate_bytes = loaded.memory_estimate_bytes();
            metrics.peak_rss_bytes = std::max(metrics.peak_rss_bytes, peak_rss_bytes());

            for (const auto& query : queries) {
                for (std::size_t repetition = 0; repetition < config.query_repetitions; ++repetition) {
                    const auto start = Clock::now();
                    const auto results = loaded.search(query);
                    metrics.query.samples.push_back(
                        std::chrono::duration<double, std::milli>(Clock::now() - start).count());
                    metrics.query_matches += results.size();
                }
            }

            if (sequential.search(queries.front()).size() != parallel.search(queries.front()).size()) {
                throw std::runtime_error("sequential and parallel index results differ");
            }
        }

        if (config.output.empty()) write_report(std::cout, config, metrics, queries);
        else {
            std::ofstream output(config.output);
            if (!output) throw std::runtime_error("Unable to open report: " + config.output.string());
            write_report(output, config, metrics, queries);
            std::cout << "Wrote report: " << config.output << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
