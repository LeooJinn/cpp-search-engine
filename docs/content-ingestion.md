# Content Ingestion

The search core accepts `Document` values and does not need to know where their text came from. The ingestion layer currently supports three source types.

## PDF

`load_pdf` invokes Poppler's `pdftotext -layout` executable and captures stdout. The path is shell-quoted, extraction is capped at 64 MiB, and a nonzero extractor exit status is reported as an error. This keeps Poppler out of the C++ link graph while using a mature PDF parser instead of attempting to parse PDF syntax in the search engine.

## Web pages

`load_web_page` uses libcurl with HTTPS support, redirects capped at five, a 20-second timeout, and a 16 MiB response cap. It accepts only HTTP(S) URLs. The current HTML conversion is intentionally conservative: tags, comments, scripts, and styles are removed and common entities are decoded. It is not a full browser parser and should not be treated as a sanitizer for rendering untrusted HTML.

The adapter is for explicit user-requested ingestion, not unrestricted crawling. It does not follow links or bypass robots controls.

## SQLite

`load_sqlite` opens a database read-only and executes a caller-provided query. The first result column becomes the document ID and the second becomes document text. This supports application-specific schemas without coupling the engine to one table layout. Results are capped at 100,000 rows and each text value at 64 MiB.

## Shared index path

Each adapter returns `std::vector<Document>`. The caller can feed that vector into sequential or parallel inverted-index construction and save the result with the versioned persistent-index format. Search, ranking, and persistence therefore behave the same regardless of whether content came from a file, PDF, website, or database.

## Next ingestion improvements

- Parse HTML with a standards-compliant parser rather than the current text-oriented stripper.
- Add content-type checks and an allowlist option for web ingestion.
- Add incremental refresh using source fingerprints and timestamps.
- Add PDF fixtures to the test corpus instead of relying only on Poppler error-path tests.
- Add query parameter binding for common SQLite ingestion patterns.
