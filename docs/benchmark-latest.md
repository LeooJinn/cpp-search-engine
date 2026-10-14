# Search Engine Benchmark

- Compiler: Apple LLVM 17.0.0 (clang-1700.0.13.5)
- Runs per size: 3
- Query repetitions per run: 10
- Parallel workers: 4
- Query: `search engine`

Times are milliseconds. Mean and median are reported across independent runs. Memory values are conservative in-process estimates, not allocator-level measurements.

| Documents | Linear build mean / median | Indexed build mean / median | Parallel build mean / median | Linear query mean / median | Indexed query mean / median | Parallel query mean / median | Indexed memory | Matches |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 500 | 0.028 / 0.010 | 8.211 / 5.567 | 2.138 / 1.635 | 59.501 / 55.438 | 0.015 / 0.016 | 0.014 / 0.015 | 3245744 | 340 |
| 2000 | 0.028 / 0.030 | 22.736 / 22.613 | 6.415 / 6.413 | 222.473 / 223.005 | 0.072 / 0.068 | 0.066 / 0.067 | 12980876 | 1340 |
| 8000 | 0.111 / 0.112 | 91.563 / 90.977 | 26.305 / 26.566 | 896.574 / 894.316 | 0.360 / 0.351 | 0.339 / 0.338 | 51921404 | 5340 |

## Reading the results

The linear engine does almost no preprocessing and pays its cost during every query. Indexed engines spend more time building postings, then answer repeated queries from those postings. Parallel construction can reduce the up-front indexing cost, but the benefit depends on corpus size, document length, worker count, and hardware. Benchmark results are workload-specific and should not be presented as universal speed guarantees.
