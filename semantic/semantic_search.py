#!/usr/bin/env python3
"""Build and query an optional Sentence Transformers semantic index."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import numpy as np
from sentence_transformers import SentenceTransformer

DEFAULT_MODEL = "sentence-transformers/all-MiniLM-L6-v2"


def read_documents(path: Path) -> list[dict[str, str]]:
    documents: list[dict[str, str]] = []
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            item = json.loads(line)
            if not isinstance(item, dict) or not item.get("id") or "text" not in item:
                raise ValueError(f"line {line_number} must contain id and text")
            documents.append(
                {
                    "id": str(item["id"]),
                    "source": str(item.get("source", "")),
                    "text": str(item["text"]),
                }
            )
    if not documents:
        raise ValueError("input JSONL contains no documents")
    return documents


def metadata_path(index_path: Path) -> Path:
    return index_path.with_suffix(index_path.suffix + ".json")


def vectors_path(index_path: Path) -> Path:
    return index_path.with_suffix(index_path.suffix + ".npz")


def build_index(input_path: Path, index_path: Path, model_name: str) -> None:
    documents = read_documents(input_path)
    model = SentenceTransformer(model_name)
    vectors = model.encode(
        [document["text"] for document in documents],
        normalize_embeddings=True,
        convert_to_numpy=True,
        show_progress_bar=True,
    ).astype(np.float32)
    index_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(vectors_path(index_path), embeddings=vectors)
    metadata_path(index_path).write_text(
        json.dumps({"model": model_name, "documents": documents}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"Wrote {len(documents)} documents to {index_path}")
    print(f"Model: {model_name}")


def query_index(index_path: Path, text: str, top_k: int, model_override: str | None) -> None:
    metadata: dict[str, Any] = json.loads(metadata_path(index_path).read_text(encoding="utf-8"))
    model_name = model_override or str(metadata["model"])
    documents = metadata["documents"]
    vectors = np.load(vectors_path(index_path))["embeddings"]
    model = SentenceTransformer(model_name)
    query_vector = model.encode([text], normalize_embeddings=True, convert_to_numpy=True)[0]
    scores = vectors @ query_vector
    ranked = np.argsort(-scores)[: max(0, top_k)]

    print(json.dumps({
        "query": text,
        "model": model_name,
        "results": [
            {
                "document_id": documents[index]["id"],
                "source": documents[index]["source"],
                "score": round(float(scores[index]), 6),
                "preview": documents[index]["text"].replace("\n", " ")[:220],
            }
            for index in ranked
        ],
    }, ensure_ascii=False, indent=2))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="embed JSONL documents")
    build.add_argument("--input", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--model", default=DEFAULT_MODEL)

    query = subparsers.add_parser("query", help="query an embedding index")
    query.add_argument("--index", type=Path, required=True)
    query.add_argument("--text", required=True)
    query.add_argument("--top-k", type=int, default=5)
    query.add_argument("--model")

    args = parser.parse_args()
    if args.command == "build":
        build_index(args.input, args.output, args.model)
    else:
        if args.top_k < 1:
            parser.error("--top-k must be positive")
        query_index(args.index, args.text, args.top_k, args.model)


if __name__ == "__main__":
    main()
