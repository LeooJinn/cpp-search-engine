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

- [ ] Create the CMake project and command-line skeleton.
- [ ] Add document loading and tokenization.
- [ ] Implement a correct baseline linear-search engine.
- [ ] Implement an inverted index and indexed queries.
- [ ] Add unit tests for parsing, indexing, queries, and invalid input.
- [ ] Add parallel indexing and compare it with the sequential version.
- [ ] Add benchmarks and publish reproducible results.
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

Build instructions will be added with the first implementation milestone.

## Benchmark plan

The benchmark suite will compare at least:

1. Linear scan versus inverted-index lookup.
2. Sequential versus parallel index construction.
3. Query latency and throughput at multiple corpus sizes.
4. Memory usage and index-size tradeoffs.

Results will be reported with the dataset size, hardware, compiler, build flags, and command used so they can be reproduced.

## Project status

Initial repository scaffold. Implementation begins with the parser, baseline search path, and test harness.
