# References and Scoping

Pegium separates name computation, scope computation, scope lookup, and linking.

## Main services

- `DefaultNameProvider`
- `DefaultScopeComputation`
- `ScopeProvider`
- `DefaultLinker`

These services answer different questions:

- what names does this document export?
- what names are visible from this reference site?
- which candidate matches the current reference text?
- how is the resolved target written back into the reference object?

## Typical flow

1. describe exported symbols for the current document
2. merge visible symbols into a scope hierarchy
3. resolve `ReferenceInfo` entries against the current scope
4. keep reference text and target tracking available for later editor features

## What a reference looks like in the AST

References are explicit AST fields:

```cpp
struct Feature : pegium::AstNode {
  string name;
  reference<Type> type;
};
```

The parser usually assigns string-like text to that field. Pegium stores the
reference text first, then resolves it later through the linker and scopes.

This is an important distinction:

- parsing records what the user wrote
- linking decides what it means

## Exported symbols vs visible symbols

Two ideas are easy to mix up:

- exported symbols: what a document contributes to the workspace index
- visible symbols: what a specific reference can see from its current location

`DefaultScopeComputation` is concerned with the first part. `ScopeProvider` is
concerned with the second part.

## Where to customize

- override the name provider if symbol naming is not a single property
- override scope computation when symbols become visible through nesting,
  imports, or custom visibility rules
- override the scope provider when lookup itself needs special logic
- keep the default linker unless the actual resolution policy must change

## Practical order of customization

In most projects, the safest order is:

1. make sure names are exported correctly
2. make sure the current scope contains the right candidates
3. only then customize the linker if the default resolution still is not enough

That avoids debugging three moving parts at once.

## Start from examples

- `domainmodel` shows qualified naming and structured scoping
- `requirements` shows multi-document and multi-language workspace behavior

## What this unlocks later

Once references and scopes are correct, several editor features become much
easier:

- go to definition
- find references
- rename
- hover over referenced targets
- diagnostics on unresolved names
