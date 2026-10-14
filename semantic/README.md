# Optional semantic-search experiment

This directory contains an optional Python sidecar for comparing embedding-based retrieval with the C++ engine's exact, fuzzy, and synonym search.

It is intentionally separate from the C++ binary: model downloads and tensor dependencies should not be required to build or run the core search engine.

## Setup

```bash
python3 -m venv .venv-semantic
source .venv-semantic/bin/activate
python -m pip install -r semantic/requirements.txt
```

The first build downloads the configured Sentence Transformers model. The default model is `sentence-transformers/all-MiniLM-L6-v2`; pass `--model` to evaluate another compatible model.

## Export documents

Build a persistent C++ index first, then export its documents as JSONL:

```bash
./build/search_cli --export-documents build/sample.index build/documents.jsonl
```

## Build and query an embedding index

```bash
python semantic/semantic_search.py build \\
  --input build/documents.jsonl \\
  --output build/semantic.index

python semantic/semantic_search.py query \\
  --index build/semantic.index \\
  --text "documents about search and databases" \\
  --top-k 5
```

The output includes cosine similarity, document ID, source, and a preview. The vectors and model name are stored beside the index so experiments can be reproduced with the same model.

## What this does and does not claim

This is an embedding-based semantic-retrieval experiment, not a replacement for the deterministic C++ index. It can retrieve conceptually related text even when exact terms do not overlap, but it introduces model downloads, latency, memory use, and model-dependent behavior. Compare it against the C++ engine on a small labeled set before making quality claims.
