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

The corpus is loaded through the production `load_directory()` path. Memory is an in-process estimate plus the peak RSS observed by `getrusage`; it is not a full allocator profile.

| Metric | Mean / value |
|---|---:|
| Sequential build (ms) | 810.784 |
| Parallel build (ms) | 433.511 |
| Index save (ms) | 187.764 |
| Index load (ms) | 106.502 |
| Query p50 (ms) | 0.005 |
| Query p95 (ms) | 0.026 |
| Query p99 (ms) | 0.027 |
| Index bytes | 140661355 |
| Estimated indexed memory | 152154620 |
| Peak RSS | 891633664 |
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
