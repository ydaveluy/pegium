# Caches

This recipe explains when to use Pegium's built-in cache types and how they fit
into the document lifecycle.

## What is the problem?

Many language services repeatedly derive the same information:

- local scope levels for one document
- filtered index views
- semantic summaries
- expensive lookup tables

Recomputing those structures on every request quickly becomes noticeable in
completion, validation, or navigation features.

## Cache types in Pegium

`src/pegium/core/utils/Caching.hpp` provides four main cache shapes:

- `SimpleCache<K, V>` for plain key-value memoization
- `ContextCache<Context, Key, Value>` for one cache per context object
- `DocumentCache<K, V>` for values tied to a `workspace::DocumentId`
- `WorkspaceCache<K, V>` for values that should be cleared when the workspace
  changes

## How to choose

Use `DocumentCache` when the computed value belongs to one document and should
be discarded automatically when that document changes.

Use `WorkspaceCache` when the value depends on the whole project and any update
should invalidate it.

Use `ContextCache` when you already have a stable context key and want one map
per context.

Use `SimpleCache` only when lifecycle-driven invalidation is not important.

## A minimal `DocumentCache` pattern

```cpp
class MySummaryService {
public:
  explicit MySummaryService(const pegium::services::SharedCoreServices &shared)
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

The key point is that `DocumentCache` is keyed by `document.id`, not by the AST
node pointer. The cache clears itself automatically when the document builder
reports that the document changed or was deleted.

## Where Pegium already uses caches

Two core services are good reference points:

- `pegium::references::DefaultScopeProvider` uses a `DocumentCache` to memoize
  local scope levels built from `document.localSymbols`
- `pegium::workspace::DefaultIndexManager` uses a `ContextCache` for typed
  export views derived from the workspace index

Those are good models to follow: cache derived structures that are expensive to
rebuild, but keep the source of truth elsewhere.

## Automatic invalidation

`DocumentCache` listens to `DocumentBuilder::onUpdate(...)` and clears only the
affected document entries.

`WorkspaceCache` also hooks into document builder events, but clears the whole
cache when the relevant workspace state changes.

That makes these caches safer than hand-rolled `std::unordered_map` members
that never hear about document updates.

## Good practices

- cache derived data, not mutable ownership-heavy objects
- keep cache keys small and stable
- prefer document-level invalidation before inventing custom cache eviction
- dispose long-lived caches when the owning service is disposed

## Related pages

- [Document Lifecycle](../../reference/document-lifecycle.md)
- [Workspace Lifecycle](../../build-a-language/workspace.md)
- [Scoping](../scoping/index.md)
