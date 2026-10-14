#!/usr/bin/env python3
"""Download a bounded Wikipedia text sample from the Hugging Face dataset API."""

from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

API = "https://datasets-server.huggingface.co/rows"
DEFAULT_DATASET = "wikimedia/wikipedia"
DEFAULT_CONFIG = "20231101.en"
DEFAULT_SPLIT = "train"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--count", type=int, default=10_000)
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--batch-size", type=int, default=100)
    parser.add_argument("--dataset", default=DEFAULT_DATASET)
    parser.add_argument("--config", default=DEFAULT_CONFIG)
    parser.add_argument("--split", default=DEFAULT_SPLIT)
    parser.add_argument("--resume", action="store_true")
    return parser.parse_args()


def fetch_rows(dataset: str, config: str, split: str, offset: int, length: int) -> dict:
    params = urllib.parse.urlencode({
        "dataset": dataset,
        "config": config,
        "split": split,
        "offset": offset,
        "length": length,
    })
    request = urllib.request.Request(
        f"{API}?{params}",
        headers={"User-Agent": "cpp-search-engine-benchmark/1.0"},
    )
    for attempt in range(6):
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            if error.code not in {429, 500, 502, 503, 504} or attempt == 5:
                raise
            retry_after = error.headers.get("Retry-After")
            delay = float(retry_after) if retry_after else min(60.0, 2.0 ** attempt * 2.0)
            print(f"HTTP {error.code}; retrying in {delay:.1f}s", flush=True)
            time.sleep(delay)
    raise RuntimeError("unreachable")


def safe_text(value: object) -> str:
    text = str(value).replace("\r\n", "\n").replace("\r", "\n").strip()
    return text


def main() -> None:
    args = parse_args()
    if args.count < 1 or args.batch_size < 1 or args.batch_size > 100:
        raise SystemExit("count must be positive and batch-size must be between 1 and 100")
    if args.output.exists() and not args.resume:
        raise SystemExit(f"refusing to write into an existing directory: {args.output}; use --resume")
    args.output.mkdir(parents=True, exist_ok=True)

    existing = sorted(args.output.glob("article-*.txt"))
    written = len(existing) if args.resume else 0
    if written > args.count:
        raise SystemExit("existing corpus is larger than requested count")
    offset = args.offset + written
    while written < args.count:
        length = min(args.batch_size, args.count - written)
        payload = fetch_rows(args.dataset, args.config, args.split, offset, length)
        rows = payload.get("rows", [])
        if not rows:
            break
        for row in rows:
            item = row.get("row", {})
            title = safe_text(item.get("title", "Untitled"))
            text = safe_text(item.get("text", ""))
            if not text:
                continue
            document = f"{title}\n\n{text}\n"
            (args.output / f"article-{written + 1:06d}.txt").write_text(document, encoding="utf-8")
            written += 1
            if written >= args.count:
                break
        offset += len(rows)
        print(f"downloaded {written}/{args.count}", flush=True)
        if len(rows) < length:
            break
        time.sleep(0.15)

    metadata = {
        "dataset": args.dataset,
        "config": args.config,
        "split": args.split,
        "start_offset": args.offset,
        "requested_count": args.count,
        "written_count": written,
        "retrieved_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "source_api": API,
    }
    (args.output / "_metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
