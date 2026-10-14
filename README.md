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
- [x] Document tradeoffs, limitations, and future improvements.
- [x] Add GitHub Actions build/test CI.
- [x] Add multi-size benchmark reporting with repeated runs, medians, and memory estimates.
- [x] Add clickable browser results with document detail views.
- [x] Add an optional embedding-based semantic-search sidecar.

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
├── docs/          # Design notes and benchmark results
├── semantic/      # Optional Python embedding-search experiment
└── .github/       # Continuous integration workflow
```

## Build

Requirements:

- CMake 3.20 or newer
- A C++17 compiler
- libcurl development files
- SQLite3 development files
- Poppler's `pdftotext` executable for PDF ingestion

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

# In-memory indexed implementation.
./build/search_cli --engine indexed data/sample "search index"

# Build a reusable on-disk index with four workers.
./build/search_cli --build-index data/sample build/sample.index --threads 4

# Reopen the saved index without rebuilding it.
./build/search_cli --index build/sample.index "search index"

# Export documents for the optional semantic-search experiment.
./build/search_cli --export-documents build/sample.index build/documents.jsonl

# Use synonym expansion and tolerate one-edit spelling mistakes.
./build/search_cli --index build/sample.index --synonyms --fuzzy 1 "automobile repar"
```

The in-memory engines use the same case-insensitive ASCII tokenization, require every query term to be present, and rank matches by the total frequency of the query terms in each document. The inverted index builds postings of the form `term -> document -> term frequency`, chooses the smallest posting list for candidates, and intersects the remaining posting lists with binary search.

The persisted format has a magic header and version number, stores document metadata and postings, writes terms in sorted order, and rejects truncated, corrupt, or unsupported-version files. This makes index reuse explicit while keeping the format intentionally small enough to inspect and test.

## Browser UI

Start the localhost-only browser interface with a persistent index:

```bash
./build/search_server build/sample.index 8080
```

Open <http://127.0.0.1:8080>. The UI calls the local `/api/search` endpoint and supports exact search, configured synonym expansion, and one-edit spelling tolerance. Search results are buttons: selecting one opens an inline document detail view with the exact matched query, source path or URL, score, character/token counts, and extracted text. The server binds only to loopback and returns at most 100 results per request.

The current query expansion is deliberately transparent lexical behavior, not neural semantic search. Synonyms come from a small checked-in dictionary and fuzzy matching uses bounded Levenshtein distance. A later optional experiment can add embedding-based retrieval without changing the core index format.


All ingestion adapters convert external content into the same `Document` type before indexing. This keeps source-specific parsing separate from tokenization and search.

```bash
# Extract text from a PDF with Poppler, then save a persistent index.
./build/search_ingest pdf report.pdf build/report.index

# Fetch an HTTP(S) page, strip HTML/script/style content, then index the text.
./build/search_ingest web https://example.com build/example.index

# Index the first two columns returned by a read-only SQLite query as ID and text.
./build/search_ingest sqlite app.db \\
  "SELECT id, body FROM documents ORDER BY id" \\
  build/database.index --threads 4
```

Web ingestion accepts only `http://` and `https://`, follows at most five redirects, applies a 20-second timeout, and caps the response at 16 MiB. SQLite opens databases read-only, caps results at 100,000 rows, and requires a query that returns document ID and document text as its first two columns. PDF extraction is delegated to Poppler's `pdftotext` executable and caps extracted text at 64 MiB.


Run the reproducible synthetic benchmark in a Release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
./build/search_benchmark --sizes 500,2000,8000 --runs 5 --query-repetitions 25 \\
  --output docs/benchmark-latest.md
```

The benchmark reports mean and median construction/query times across repeated runs, estimated memory usage, and match totals for multiple corpus sizes. It verifies that the linear, sequential-indexed, and parallel-indexed engines return the same matches before producing a report. `docs/benchmark-latest.md` is a generated snapshot; benchmark results are workload-specific and should not be presented as universal speed guarantees.

## Continuous integration

GitHub Actions builds and tests the project on Ubuntu and macOS for pushes and pull requests. CI also runs a small benchmark smoke test. The workflow is in [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

## Optional semantic search

The C++ engine remains deterministic and dependency-light. The optional `semantic/` sidecar uses NumPy and Sentence Transformers to build a cosine-similarity embedding index, allowing a separate comparison between exact, fuzzy, synonym, and semantic retrieval:

```bash
python3 -m venv .venv-semantic
source .venv-semantic/bin/activate
python -m pip install -r semantic/requirements.txt
./build/search_cli --export-documents build/sample.index build/documents.jsonl
python semantic/semantic_search.py build --input build/documents.jsonl --output build/semantic.index
python semantic/semantic_search.py query --index build/semantic.index --text "documents about search" --top-k 5
```

This experiment is intentionally not required for the C++ build and does not claim that a small embedding model matches a production search service.

See [`docs/design-and-tradeoffs.md`](docs/design-and-tradeoffs.md) for the data flow, indexing strategy, concurrency design, correctness approach, and limitations. See [`docs/benchmark-results.md`](docs/benchmark-results.md) for a recorded benchmark run and its caveats. See [`docs/benchmark-latest.md`](docs/benchmark-latest.md) for the current multi-size report. See [`docs/content-ingestion.md`](docs/content-ingestion.md) for PDF, web, and SQLite ingestion details. See [`docs/query-features.md`](docs/query-features.md) for fuzzy matching, synonym expansion, and the semantic-search boundary. See [`semantic/README.md`](semantic/README.md) for the optional embedding experiment.

## Benchmark plan

The benchmark suite will compare at least:

1. Linear scan versus inverted-index lookup.
2. Sequential versus parallel index construction.
3. Query latency and throughput at multiple corpus sizes.
4. Memory usage and index-size tradeoffs.

Results will be reported with the dataset size, hardware, compiler, build flags, and command used so they can be reproduced.

## Project status

Milestone 9 is complete: the project now has GitHub Actions CI, multi-size repeated benchmark reports with median and memory estimates, clickable browser results with document detail views, and an optional embedding-based semantic-search sidecar. The C++ core remains the canonical exact/fuzzy/synonym engine; semantic retrieval is isolated for honest comparison and reproducible model experiments.
