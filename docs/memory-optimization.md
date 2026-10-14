# Memory Optimization Benchmark

## Goal

The first real-corpus run showed excellent query latency but a large memory footprint. On 5,950 Wikipedia articles and 74.5 MB of source text, peak RSS reached 1.04 GB. The main build-time allocation problem was the parallel indexer retaining one `unordered_map` of term counts per document.

## Change

The parallel builder now:

1. Reuses one temporary term-count map per worker.
2. Appends each document's counts directly into one worker-local postings map.
3. Merges worker postings into the shared inverted index in document-range order.
4. Releases worker-local maps after merging.

This preserves search results and deterministic postings while removing thousands of per-document hash-map allocations and repeated term-key storage.

## Identical workload

- Corpus: 5,950 English Wikipedia articles
- Source text: 74,507,135 bytes
- Query set: 11 fixed queries in `docs/benchmark-queries.txt`
- Build runs: 3
- Query repetitions per run: 3
- Parallel workers: 4
- Build path: production `load_directory()` and `IndexedSearchEngine`
- Machine: Apple Silicon development machine

## Before and after

| Metric | Before | After | Change |
|---|---:|---:|---:|
| Sequential build | 774.648 ms | 810.784 ms | +4.7% |
| Parallel build | 496.205 ms | 433.511 ms | **12.6% faster** |
| Index save | 220.283 ms | 187.764 ms | **14.8% faster** |
| Index load | 133.020 ms | 106.502 ms | **19.9% faster** |
| Query p50 | 0.005 ms | 0.005 ms | unchanged |
| Query p95 | 0.028 ms | 0.026 ms | **7.1% faster** |
| Query p99 | 0.033 ms | 0.027 ms | **18.2% faster** |
| Persistent index | 140,655,405 B | 140,661,355 B | effectively unchanged |
| Estimated indexed memory | 152,154,620 B | 152,154,620 B | unchanged |
| Peak RSS | 1,044,480,000 B | 891,633,664 B | **14.6% lower** |

The estimated steady-state index memory is unchanged because the serialized postings format and in-memory index representation are unchanged. The improvement is primarily in peak build-time memory and allocation behavior.

## Interpretation

This optimization did not pretend to solve every scale problem. The engine still retains full document text and an in-memory index, so the next memory reduction would require separating searchable metadata from document bodies and loading text lazily for browser detail views. The current result is a measured improvement with preserved correctness, not a claim of production-scale readiness.
