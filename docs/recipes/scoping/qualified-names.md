# Qualified Names

Qualified name scoping lets users reference declarations through names such as
`billing.Invoice` or `company.sales.Customer`.

Pegium's `domainmodel` example uses this pattern for packages and types. The
important idea is that globally exported names can be qualified while local
lookup can still keep short names available inside the right container.

## What is the problem?

Qualified names are useful as soon as one short name is no longer enough. Once
users can write names such as `blog.User` or `sales.Invoice`, the language can
disambiguate declarations without forcing everything into one flat namespace.

At the same time, users usually still expect local short names to work inside
the right container. That means qualified names are rarely just a lookup
problem. They are usually a question of how symbols are exported and how local
symbols are precomputed.

## What needs to change?

For qualified names, you usually do not replace the linker. Instead, you
customize the exported names and the local symbols that the default scope
provider later consumes.

The `domainmodel` example does this with:

- a `QualifiedNameProvider`
- a custom `DomainModelScopeComputation`
- the existing default scope provider

This approach is also usually cheaper than a heavily custom scope lookup,
because most of the work is done once per document during scope computation
instead of once per reference during completion or linking.

## Step 1: define how names are joined

`examples/domainmodel/src/references/QualifiedNameProvider.cpp` keeps the rule
small and explicit:

```cpp
std::string QualifiedNameProvider::getQualifiedName(std::string_view qualifier,
                                                    std::string_view name) const {
  if (qualifier.empty()) {
    return std::string(name);
  }
  return std::string(qualifier) + "." + std::string(name);
}
```

That helper is also used recursively for nested packages.

## Step 2: export global symbols under qualified names

`DomainModelScopeComputation::collectExportedSymbols(...)` walks the model,
tracks the current qualifier, and publishes types under their fully qualified
name:

```cpp
if (const auto *package =
        dynamic_cast<const PackageDeclaration *>(element.get())) {
  const auto nestedQualifier =
      _qualifiedNameProvider != nullptr
          ? _qualifiedNameProvider->getQualifiedName(qualifier, package->name)
          : package->name;
  collectExportedSymbols(package->elements, nestedQualifier, document, symbols,
                         cancelToken);
  continue;
}
```

Later, when a type description is created, the exported name becomes something
like `foo.bar.Customer`.

## Step 3: keep local names usable inside containers

Only exporting qualified names is not enough. Inside a package, users still
expect short names to work.

The same example therefore overrides `collectLocalSymbols(...)` and builds
local descriptions per container. Nested descriptions are re-added with a
qualified form where needed, then stored in `document.localSymbols`.

That is the data structure consumed later by the default scope provider.

## Why the default scope provider still works

`pegium::references::DefaultScopeProvider` already knows how to combine:

- local symbols from the current container chain
- exported symbols from the workspace index

So once your scope computation emits the right names, the default lookup logic
often remains sufficient.

This is why qualified-name support is usually a scope-computation problem
rather than a linker problem.

## When to go further

If your language also needs imports, visibility modifiers, or access rules that
depend on the reference site, continue with
[Custom Scope Provider](../custom-scope-provider.md).

## Related pages

- [Examples: DomainModel](../../examples/domainmodel.md)
- [Custom Scope Provider](../custom-scope-provider.md)
- [Workspace Lifecycle](../../build-a-language/workspace.md)
