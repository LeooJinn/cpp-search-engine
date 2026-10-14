# Real Corpus Search Benchmark

- Corpus: `data/corpora/wikipedia-5950`
- Queries: `docs/benchmark-queries.txt`
- Documents: 5950
- Corpus bytes: 74507135
- Query count: 11
- Token count: 11987804
- Unique terms: 240613
- Build runs: 3
- Query repetitions per run: 3
- Parallel workers: 4

Source: `wikimedia/wikipedia`, config `20231101.en`, split `train`, retrieved through the Hugging Face dataset server rows API. The downloader was rate-limited after 5,950 rows; the exact local provenance is recorded in the ignored corpus metadata file.
The corpus is loaded through the production `load_directory()` path. Memory is an in-process estimate plus the peak RSS observed by `getrusage`; it is not a full allocator profile.

| Metric | Mean / value |
|---|---:|
| Sequential build (ms) | 774.648 |
| Parallel build (ms) | 496.205 |
| Index save (ms) | 220.283 |
| Index load (ms) | 133.020 |
| Query p50 (ms) | 0.005 |
| Query p95 (ms) | 0.028 |
| Query p99 (ms) | 0.033 |
| Index bytes | 140655405 |
| Estimated indexed memory | 152154620 |
| Peak RSS | 1044480000 |
| Total query matches | 11592 |

## Query set

- `computer science`
- `database systems`
- `distributed computing`
- `machine learning`
- `operating system`
- `network protocol`
- `quantum mechanics`
- `software engineering`
- `climate change`
- `astronomical object`
- `this query should not match any document xyzzy123`
