#include "search_engine/document.h"
#include "search_engine/indexed_search.h"
#include "search_engine/linear_search.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct TimingStats {
    std::vector<double> samples;

    [[nodiscard]] double mean() const {
        return std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    }

    [[nodiscard]] double median() const {
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const auto middle = sorted.size() / 2;
        if (sorted.size() % 2 == 0) {
            return (sorted[middle - 1] + sorted[middle]) / 2.0;
        }
        return sorted[middle];
    }
};

struct BenchmarkResult {
    std::size_t document_count;
    TimingStats linear_build;
    TimingStats indexed_build;
    TimingStats parallel_build;
    TimingStats linear_query;
    TimingStats indexed_query;
    TimingStats parallel_query;
    std::size_t linear_memory_bytes;
    std::size_t indexed_memory_bytes;
    std::size_t parallel_memory_bytes;
    std::size_t total_matches;
};

struct Config {
    std::vector<std::size_t> sizes{500, 2'000, 8'000};
    std::size_t runs = 5;
    std::size_t query_repetitions = 25;
    std::size_t parallel_threads = 4;
    std::string query = "search engine";
    std::string output_path;
};

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

std::vector<std::size_t> parse_sizes(std::string_view value) {
    std::vector<std::size_t> sizes;
    std::string input(value);
    std::stringstream stream(input);
    std::string part;
    while (std::getline(stream, part, ',')) {
        const auto size = std::stoull(part);
        if (size == 0) {
            throw std::invalid_argument("benchmark sizes must be positive");
        }
        sizes.push_back(static_cast<std::size_t>(size));
    }
    if (sizes.empty()) {
        throw std::invalid_argument("at least one benchmark size is required");
    }
    return sizes;
}

std::size_t parse_positive(std::string_view flag, std::string_view value) {
    const auto number = std::stoull(std::string(value));
    if (number == 0) {
        throw std::invalid_argument(std::string(flag) + " must be positive");
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

        if (argument == "--sizes") {
            config.sizes = parse_sizes(require_value("--sizes"));
        } else if (argument == "--runs") {
            config.runs = parse_positive("--runs", require_value("--runs"));
        } else if (argument == "--query-repetitions") {
            config.query_repetitions = parse_positive(
                "--query-repetitions", require_value("--query-repetitions"));
        } else if (argument == "--threads") {
            config.parallel_threads = parse_positive("--threads", require_value("--threads"));
        } else if (argument == "--query") {
            config.query = std::string(require_value("--query"));
        } else if (argument == "--output") {
            config.output_path = std::string(require_value("--output"));
        } else if (argument == "--help") {
            std::cout << "Usage: search_benchmark [options]\n"
                         "  --sizes 500,2000,8000\n"
                         "  --runs 5\n"
                         "  --query-repetitions 25\n"
                         "  --threads 4\n"
                         "  --query \"search engine\"\n"
                         "  --output docs/benchmark-latest.md\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    return config;
}

BenchmarkResult run_benchmark(const Config& config, std::size_t document_count) {
    BenchmarkResult result{
        document_count,
        {}, {}, {}, {}, {}, {},
        0, 0, 0, 0,
    };
    for (std::size_t run = 0; run < config.runs; ++run) {
        auto [linear_build_ms, linear] = build_engine<search_engine::LinearSearchEngine>(
            make_corpus(document_count));
        auto [indexed_build_ms, indexed] = build_engine<search_engine::IndexedSearchEngine>(
            make_corpus(document_count));
        auto [parallel_build_ms, parallel] = build_parallel_engine(
            make_corpus(document_count), config.parallel_threads);

        const auto [linear_query_ms, linear_matches] =
            benchmark_queries(linear, config.query, config.query_repetitions);
        const auto [indexed_query_ms, indexed_matches] =
            benchmark_queries(indexed, config.query, config.query_repetitions);
        const auto [parallel_query_ms, parallel_matches] =
            benchmark_queries(parallel, config.query, config.query_repetitions);

        if (linear_matches != indexed_matches || indexed_matches != parallel_matches) {
            throw std::runtime_error("search implementations returned different match totals");
        }
        result.linear_build.samples.push_back(linear_build_ms);
        result.indexed_build.samples.push_back(indexed_build_ms);
        result.parallel_build.samples.push_back(parallel_build_ms);
        result.linear_query.samples.push_back(linear_query_ms);
        result.indexed_query.samples.push_back(indexed_query_ms);
        result.parallel_query.samples.push_back(parallel_query_ms);
        result.linear_memory_bytes = linear.memory_estimate_bytes();
        result.indexed_memory_bytes = indexed.memory_estimate_bytes();
        result.parallel_memory_bytes = parallel.memory_estimate_bytes();
        result.total_matches = linear_matches;
    }
    return result;
}

void write_report(std::ostream& output, const Config& config,
                  const std::vector<BenchmarkResult>& results) {
    output << "# Search Engine Benchmark\n\n"
           << "- Compiler: " << __VERSION__ << "\n"
           << "- Runs per size: " << config.runs << "\n"
           << "- Query repetitions per run: " << config.query_repetitions << "\n"
           << "- Parallel workers: " << config.parallel_threads << "\n"
           << "- Query: `" << config.query << "`\n\n"
           << "Times are milliseconds. Mean and median are reported across independent runs. "
              "Memory values are conservative in-process estimates, not allocator-level measurements.\n\n"
           << "| Documents | Linear build mean / median | Indexed build mean / median | Parallel build mean / median | "
              "Linear query mean / median | Indexed query mean / median | Parallel query mean / median | "
              "Indexed memory | Matches |\n"
           << "|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";

    output << std::fixed << std::setprecision(3);
    for (const auto& result : results) {
        output << "| " << result.document_count
               << " | " << result.linear_build.mean() << " / " << result.linear_build.median()
               << " | " << result.indexed_build.mean() << " / " << result.indexed_build.median()
               << " | " << result.parallel_build.mean() << " / " << result.parallel_build.median()
               << " | " << result.linear_query.mean() << " / " << result.linear_query.median()
               << " | " << result.indexed_query.mean() << " / " << result.indexed_query.median()
               << " | " << result.parallel_query.mean() << " / " << result.parallel_query.median()
               << " | " << result.indexed_memory_bytes
               << " | " << result.total_matches << " |\n";
    }

    output << "\n## Reading the results\n\n"
           << "The linear engine does almost no preprocessing and pays its cost during every query. "
              "Indexed engines spend more time building postings, then answer repeated queries from those postings. "
              "Parallel construction can reduce the up-front indexing cost, but the benefit depends on corpus size, "
              "document length, worker count, and hardware. Benchmark results are workload-specific and should not be "
              "presented as universal speed guarantees.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto config = parse_arguments(argc, argv);
        std::vector<BenchmarkResult> results;
        results.reserve(config.sizes.size());
        for (const auto document_count : config.sizes) {
            results.push_back(run_benchmark(config, document_count));
        }

        if (config.output_path.empty()) {
            write_report(std::cout, config, results);
        } else {
            std::ofstream output(config.output_path);
            if (!output) {
                throw std::runtime_error("Unable to open benchmark report: " + config.output_path);
            }
            write_report(output, config, results);
            std::cout << "Wrote benchmark report: " << config.output_path << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
