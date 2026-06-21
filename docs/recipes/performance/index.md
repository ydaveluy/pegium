# Performance

Keep expensive language services responsive as the workspace grows. The first tool to reach for is usually caching.

## Typical cases

- computing document-local symbol tables
- memoizing derived semantic data
- reusing per-type index views
- avoiding repeated whole-workspace scans

## Related pages

- [Caches](caches.md) — start here
- [Document Lifecycle](../../reference/document-lifecycle.md) — reason about when cached data becomes stale
