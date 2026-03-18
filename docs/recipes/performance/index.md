# Performance

Performance recipes are about keeping expensive language services responsive as
the workspace grows.

In Pegium, the first performance tool to reach for is usually caching.

## Typical cases

- computing document-local symbol tables
- memoizing derived semantic data
- reusing per-type index views
- avoiding repeated whole-workspace scans

Start with [Caches](caches.md), then return to the
[Document Lifecycle](../../reference/document-lifecycle.md) page if you need to
reason about when cached data becomes stale.
