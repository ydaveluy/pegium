# Custom Scope Provider

Customize scoping when visibility rules are more complex than lexical nesting.

If your language only needs package-style or namespace-style qualified names,
start with [Qualified Names](scoping/qualified-names.md). Reach for a custom
scope provider only when lookup behavior itself must change.

## Main entry points

- custom name provider
- custom scope computation
- custom scope provider

Each one solves a different problem:

- name provider: how declarations are named
- scope computation: which symbols are exported and indexed
- scope provider: which symbols are visible at a given reference site

In practice, a custom name provider usually customizes both:

- `getName(...)` for the exported symbol key
- `getNameNode(...)` for declaration ranges used by editor features

When your AST already stores declaration names in a shared base type, prefer
the recommended naming pattern described in
[References and Scoping](../build-a-language/references-and-scoping.md#recommended-naming-pattern).

## Scope provider contract

A `pegium::references::ScopeProvider` exposes two operations on a reference
site described by `ReferenceInfo` (which carries the reference text, the
container AST node, and the type of the expected target):

- `getScopeEntry(...)` returns the first visible
  `AstNodeDescription` matching the reference text, or `nullptr` if no
  candidate exists. This is the fast path used by the linker.
- `visitScopeEntries(...)` enumerates visible candidates in lexical order
  and stops as soon as the visitor returns `false`. This is the path used
  by completion.

Both operations should agree on visibility: if a description is reachable
through `visitScopeEntries(...)`, an exact-match lookup with the same
reference text must also return it through `getScopeEntry(...)`.

## Skeleton of a custom scope provider

The simplest customization derives from `DefaultScopeProvider` and
overrides only one of the two methods:

```cpp
class MyScopeProvider final : public pegium::references::DefaultScopeProvider {
public:
  using DefaultScopeProvider::DefaultScopeProvider;

  bool visitScopeEntries(
      const pegium::ReferenceInfo &context,
      pegium::utils::function_ref<
          bool(const pegium::workspace::AstNodeDescription &)>
          visitor) const override {
    // Inject any extra entries that should be visible at this site, then
    // fall back to the default visibility chain (local symbols + index).
    if (auto *extra = my_extra_entry_for(context); extra != nullptr) {
      if (!visitor(*extra)) {
        return false;
      }
    }
    return DefaultScopeProvider::visitScopeEntries(context, visitor);
  }
};
```

Wire it like any other service:

```cpp
services->references.scopeProvider =
    std::make_unique<MyScopeProvider>(*services);
```

## A practical order

1. keep the default linker
2. customize exported names if needed
3. customize scope computation for visibility
4. customize the scope provider only when lookup itself needs a special rule

## Typical reasons

- imports
- namespaces
- qualified names
- visibility modifiers
- multi-file symbol aggregation

## Practical advice

Most scoping bugs are easier to diagnose if you first verify exported symbols,
then visible symbols, then final linker behavior. Try not to change all three
layers at once.

## Related pages

- [Scoping](scoping/index.md)
- [Qualified Names](scoping/qualified-names.md)
