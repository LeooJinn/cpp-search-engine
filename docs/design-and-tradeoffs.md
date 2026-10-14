# Design and Tradeoffs

## Problem

The project searches a directory of plain-text documents for queries containing one or more terms. A query is an AND query: a document must contain every distinct normalized query term to be returned. Results are ranked by the sum of each query term's frequency in the document.

The implementation deliberately contains a simple baseline and optimized alternatives so their behavior and performance can be compared.

## Data flow

```text
Directory
  -> document loader
  -> Document{id, path, text}
  -> tokenizer
  -> search engine
  -> ranked SearchResult values
```

The document loader sorts regular files by path before returning them. This gives each run a deterministic document order and makes benchmark output reproducible.

The tokenizer currently performs ASCII normalization: alphanumeric characters become lowercase token characters, while punctuation and whitespace delimit tokens. This is intentionally small and predictable; Unicode tokenization and stemming are future work.

## Linear baseline

`LinearSearchEngine` stores the original documents. For each query it:

1. Tokenizes and deduplicates the query terms.
2. Tokenizes each document.
3. Counts terms in that document.
4. Keeps the document only if every query term is present.
5. Sorts results by descending score and then document ID.

This is easy to validate and is a useful correctness oracle. Its repeated-query cost grows with the amount of document text because it reprocesses every document for every query.

## Inverted index

`IndexedSearchEngine` preprocesses each document once and stores postings in this shape:

```text
term -> [(document index, term frequency), ...]
```

The postings are appended in document order. To answer a query, the engine:

1. Looks up the posting list for each distinct query term.
2. Returns immediately if any term is unknown.
3. Starts with the smallest posting list to minimize candidates.
4. Intersects the remaining posting lists using binary search by document index.
5. Sums term frequencies and applies the same result ordering as the baseline.

The index uses extra memory and adds build time, but avoids rescanning and retokenizing the corpus for every query.

## Parallel indexing

`add_documents_parallel` divides the input documents into contiguous ranges. Each worker builds term counts for its assigned documents in a private map, so workers do not concurrently mutate the shared inverted index. After all workers join, the main thread moves documents into storage and merges the per-document counts in document order.

This design favors correctness and deterministic output over maximum parallelism. It avoids locks during tokenization and index writes, but the final merge is sequential. For very small corpora, thread startup and merge overhead can outweigh the benefit.

## Correctness strategy

The test executable checks:

- Case normalization and punctuation handling
- Empty input
- AND semantics for multi-term queries
- Frequency-based ranking
- Duplicate query-term handling
- Deterministic file loading
- Missing-directory errors
- Equivalence of linear and indexed results
- Equivalence of sequential and parallel indexed results
- Invalid parallel thread counts

The benchmark also compares match totals from all implementations and exits unsuccessfully if they disagree.

## Known limitations

- Only regular plain-text files are supported.
- Tokenization is ASCII-oriented and does not understand Unicode word boundaries.
- There is no stemming, synonym expansion, typo correction, phrase search, or semantic search.
- The ranking function is term frequency, not TF-IDF or BM25.
- The index is built in memory and is not persisted between runs.
- The current CLI accepts one query and exits; it does not serve a long-running query process.
- The parallel implementation uses a fixed caller-provided worker count and does not yet expose a thread-pool abstraction.

These limitations are intentional scope boundaries for the current portfolio project.
