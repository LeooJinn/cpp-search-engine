# C++ Search Engine

A portfolio-grade document search engine built in C++17, focused on data structures, concurrency, testing, and measurable performance.

The project will compare a straightforward linear-scan implementation with an indexed implementation, then explore parallel indexing and query processing. The goal is a small system that is easy to explain in a software-engineering interview and rigorous enough to benchmark.

## Goals

- Parse a directory of plain-text documents.
- Normalize and tokenize document contents.
- Build an inverted index for fast keyword lookup.
- Support single-term and multi-term queries.
- Provide a simple command-line interface.
- Compare linear scanning against indexed search.
- Add parallel indexing and query processing where it improves throughput.
- Test correctness, malformed input, empty queries, and edge cases.
- Record reproducible latency, throughput, memory, and speedup measurements.

## Planned technical focus

- C++17 and the standard library
- Data structures: hash tables, sets, vectors, and inverted indexes
- Algorithms: tokenization, filtering, lookup, ranking, and intersection
- Concurrency: worker threads and bounded work queues
- Engineering: CMake, unit tests, benchmarks, documentation, and profiling

## Milestones

- [x] Create the CMake project and command-line skeleton.
- [x] Add document loading and tokenization.
- [x] Implement a correct baseline linear-search engine.
- [x] Implement an inverted index and indexed queries.
- [x] Add unit tests for parsing, indexing, queries, and invalid input.
- [x] Add benchmarks and publish reproducible results.
- [x] Add parallel indexing and compare it with the sequential version.
- [ ] Document tradeoffs, limitations, and future improvements.

## Proposed layout

```text
.
├── CMakeLists.txt
├── README.md
├── include/       # Public headers
├── src/           # Production implementation
├── tests/         # Correctness and regression tests
├── benchmarks/    # Performance measurements
├── data/          # Small local sample corpus only
└── docs/          # Design notes and benchmark results
```

## Build

Requirements:

- CMake 3.20 or newer
- A C++17 compiler

Configure and build from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run either search implementation against the included sample corpus:

```bash
# Linear baseline: scans and tokenizes every document for each query.
./build/search_cli --engine linear data/sample "search index"

# Indexed implementation: builds an inverted index, then looks up query terms.
./build/search_cli --engine indexed data/sample "search index"
```

The engines use the same case-insensitive ASCII tokenization, require every query term to be present, and rank matches by the total frequency of the query terms in each document. The indexed implementation builds postings of the form `term -> document -> term frequency`, chooses the smallest posting list for candidates, and intersects the remaining posting lists with binary search.

Run the reproducible synthetic benchmark in a Release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
./build/search_benchmark
```

The benchmark reports construction time, repeated-query time, and total matches for the linear, sequential-indexed, and parallel-indexed engines. It generates a deterministic 2,000-document corpus and repeats the same query 25 times; the match totals must agree before the benchmark exits successfully. Construction is expected to be more expensive for indexed engines because they do work up front, while repeated queries should be substantially cheaper. Parallel indexing partitions documents across workers, builds per-document term counts without shared writes, and merges postings in document order.

See [`docs/design-and-tradeoffs.md`](docs/design-and-tradeoffs.md) for the data flow, indexing strategy, concurrency design, correctness approach, and limitations. See [`docs/benchmark-results.md`](docs/benchmark-results.md) for a recorded benchmark run and its caveats.

## Benchmark plan

The benchmark suite will compare at least:

1. Linear scan versus inverted-index lookup.
2. Sequential versus parallel index construction.
3. Query latency and throughput at multiple corpus sizes.
4. Memory usage and index-size tradeoffs.

Results will be reported with the dataset size, hardware, compiler, build flags, and command used so they can be reproduced.

## Project status

Milestone 5 is complete: the project now has design and tradeoff documentation, a recorded benchmark run with environment details and caveats, and verified linear, sequential-indexed, and parallel-indexed implementations. Future improvements include larger workloads, memory measurement, persistent indexes, and richer ranking.
