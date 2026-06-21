# Caches

Reuse Pegium's built-in caches instead of recomputing the same derived data on every request, and let them invalidate themselves as documents change.

## What is the problem?

Many language services repeatedly derive the same information:

- local scope levels for one document
- filtered index views
- semantic summaries
- expensive lookup tables

Recomputing these on every completion, validation, or navigation request adds up fast.

## Cache types in Pegium

`src/pegium/core/utils/Caching.hpp` provides four cache shapes:

- `SimpleCache<K, V>` — plain key-value memoization
- `ContextCache<Context, Key, Value>` — one cache per context object
- `DocumentCache<K, V>` — values tied to a `workspace::DocumentId`
- `WorkspaceCache<K, V>` — values cleared when the workspace changes

## How to choose

| Cache | Use when |
| --- | --- |
| `DocumentCache` | the value belongs to one document and should be discarded when that document changes |
| `WorkspaceCache` | the value depends on the whole project and any update should invalidate it |
| `ContextCache` | you already have a stable context key and want one map per context |
| `SimpleCache` | lifecycle-driven invalidation does not matter |

## A minimal `DocumentCache` pattern

```cpp
class MySummaryService {
public:
  explicit MySummaryService(const pegium::SharedCoreServices &shared)
      : _cache(shared) {}

  std::vector<std::string> summary(const pegium::AstNode &node) const {
    const auto &document = pegium::getDocument(node);
    return _cache.get(document.id, static_cast<std::uint8_t>(0), [&]() {
      return computeSummary(document);
    });
  }

private:
  static std::vector<std::string>
  computeSummary(const pegium::workspace::Document &document);

  mutable pegium::utils::DocumentCache<std::uint8_t, std::vector<std::string>>
      _cache;
};
```

`DocumentCache` is keyed by `document.id`, not by the AST node pointer. It clears itself when the document builder reports the document changed or was deleted.

## Where Pegium already uses caches

`pegium::workspace::DefaultIndexManager` uses a `ContextCache` for typed export views derived from the workspace index.

Follow that model: cache derived structures that are expensive to rebuild, but keep the source of truth elsewhere. `pegium::references::DefaultScopeProvider` caches its per-reference-type *global* (workspace-wide exported) scope view in a `WorkspaceCache`. Its *local* scope levels are precomputed and bucketed by container at scope-computation time, so local lookups are direct index accesses and need no separate cache.

## Automatic invalidation

- `DocumentCache` listens to `DocumentBuilder::onUpdate(...)` and clears only the affected document entries.
- `WorkspaceCache` also hooks into document builder events, but clears the whole cache when the relevant workspace state changes.

This makes both safer than hand-rolled `std::unordered_map` members that never hear about document updates.

## Practical advice

- cache derived data, not mutable ownership-heavy objects
- keep cache keys small and stable
- prefer document-level invalidation before inventing custom cache eviction
- dispose long-lived caches when the owning service is disposed

## Related pages

- [Document Lifecycle](../../reference/document-lifecycle.md)
- [Workspace Lifecycle](../../build-a-language/workspace.md)
- [Scoping](../scoping/index.md)
