# Query Features

## Exact search

The default mode tokenizes the query, requires every distinct term to occur in a document, and ranks by the sum of matching term frequencies.

## Synonym expansion

`SearchOptions::expand_synonyms` expands a query term using the checked-in dictionary in `src/synonyms.cpp`. A query term becomes a small OR group while the original query remains an AND across groups. For example, `car repair` can match a document containing `automobile repair` when expansion is enabled.

This is transparent lexical expansion, not a learned language model. The dictionary is intentionally small and should grow only with documented, tested terms.

## Fuzzy matching

`SearchOptions::allow_fuzzy` compares each query term with the indexed vocabulary using bounded Levenshtein distance. A maximum edit distance is supplied by the caller. Matching vocabulary terms form an OR group for that query term.

Fuzzy matching is more expensive than exact lookup because it scans the vocabulary for each query term. It is appropriate for short interactive queries and small-to-medium vocabularies; a production-scale implementation would use a trie, BK-tree, or another approximate-nearest-neighbor structure.

## Semantic-search boundary

Modern semantic search normally uses embeddings: a model maps documents and queries into vectors, and retrieval compares vector similarity. This project does not currently claim to do that. Keeping the boundary explicit avoids presenting a synonym table or edit distance as neural semantic understanding.

An embedding-based experiment can be added as an optional sidecar later. It should have its own model/version metadata, evaluation corpus, and fallback behavior so exact search remains deterministic and dependency-light.
