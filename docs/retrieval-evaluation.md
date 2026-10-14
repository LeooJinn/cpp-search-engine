# Retrieval evaluation set

This small, hand-labeled corpus is for a qualitative comparison of the retrieval modes. It is deliberately tiny; the results are a demonstration, not a benchmark of search quality.

| Query | Mode | Observed top document | Result |
|---|---|---|---|
| `car repair` | exact | no result | Pass: the corpus uses `automobile repair`, not the literal term `car`. |
| `car repair` | synonym expansion | `vehicles.txt` | Pass: `car` expands to `automobile`/`vehicle`. |
| `algoritms` | fuzzy distance 1 | `algorithms.txt` | Pass: one deletion from `algorithms`. |
| `reliable data storage` | semantic | `storage.txt` | Pass: `all-MiniLM-L6-v2` ranked `storage.txt` first with cosine score `0.714631`. |

Build the evaluation index:

```bash
./build/search_cli --build-index data/evaluation build/evaluation.index --threads 4
```

The first three cases are run by the C++ CLI. The semantic case is run by exporting the documents and querying the optional Python sidecar. The evaluation is qualitative and intentionally does not claim general search quality.
