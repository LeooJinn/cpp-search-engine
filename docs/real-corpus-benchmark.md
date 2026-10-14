# Real corpus benchmark

The benchmark uses a local corpus directory and a fixed one-query-per-line workload. It measures the production `load_directory()` path, sequential and parallel index construction, persistent index save/load, per-query latency percentiles, index bytes, estimated indexed memory, and peak resident memory.

## Download the Wikipedia sample

The downloader fetches a bounded sample from the public Hugging Face dataset server view of Wikimedia Wikipedia. It writes one article per text file and never executes the downloaded content.

```bash
python3 -I tools/download_wikipedia_sample.py \\
  --output data/corpora/wikipedia-5950 \\
  --count 10000 \\
  --batch-size 100
```

The downloader can be resumed after a public API rate limit. This run completed with 5,950 articles before the API rate-limited further requests; the exact corpus provenance is recorded in `data/corpora/wikipedia-5950/_metadata.json` locally.

## Run it

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel

./build/search_real_benchmark \\
  --corpus data/corpora/wikipedia-5950 \\
  --queries docs/benchmark-queries.txt \\
  --threads 4 \\
  --runs 3 \\
  --query-repetitions 3 \\
  --output docs/wikipedia-5950-benchmark.md \\
  --index-output build/wikipedia-5950.index
```

The output report should include the exact document count, corpus bytes, token and vocabulary estimates, mean build/save/load times, p50/p95/p99 query latency, persistent index size, estimated indexed memory, and peak RSS.

## Scaling up

After the 5,950-article run succeeds, resume the downloader with a larger local sample if the public API allows it:

```bash
python3 -I tools/download_wikipedia_sample.py \\
  --output data/corpora/wikipedia-5950 \\
  --count 10000 \\
  --batch-size 50 \\
  --resume
```

Do not claim that the project handles production-scale search simply because a larger sample completes. Record where memory, build time, or query latency starts to strain the current in-memory architecture. That bottleneck is the next optimization target.
