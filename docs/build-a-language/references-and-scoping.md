# References and Scoping

Pegium separates name computation, scope computation, scope lookup, and linking.

## Main services

- `NameProvider`
- `ScopeComputation`
- `ScopeProvider`
- `Linker`

These services answer different questions:

- what names does this document export?
- what names are visible from this reference site?
- which candidate matches the current reference text?
- how is the resolved target written back into the reference object?

`NameProvider` itself is intentionally split in two:

- `getName(...)` returns the exported symbol name
- `getNameNode(...)` returns the CST node that should be used for declaration-site navigation

That split lets scoping use canonicalized names while editor features still
point at the exact source range of the declaration.

For editor-facing code, Pegium also exposes small helpers on top of that split:

- `named_node_info(...)` when a feature needs the name plus both relevant CST ranges
- `declaration_site_node(...)` / `required_declaration_site_node(...)` when a feature only needs the declaration-site range

## Recommended naming pattern

When your language has several declaration nodes with the same `name` property,
it is recommended to inherit them from the built-in `pegium::NamedAstNode`.

```cpp
struct Entity : pegium::NamedAstNode {};
struct DataType : pegium::NamedAstNode {};
struct PackageDeclaration : pegium::NamedAstNode {};
```

The default naming service reads `pegium::NamedAstNode::name` directly from the
AST. This is usually the best default because:

- names come from the semantic model instead of being re-read from the CST
- one shared base avoids duplicating the same `name` field across declarations
- declaration-site navigation still points at the original source text

## Typical flow

1. describe exported symbols for the current document
2. enumerate visible symbols from `ScopeProvider`
3. resolve `ReferenceInfo` entries against those visible symbols
4. keep reference text and target tracking available for later editor features

`ReferenceInfo` is assignment-backed: it keeps the reference text plus the
grammar `Assignment` that defines the feature name and expected target type.

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

## `AstNodeDescription` contract

Runtime symbol descriptions are not partial records.

- exported and local descriptions must carry a valid `documentId`
- they must also carry a valid `symbolId`
- they must carry a non-zero `nameLength` covering the visible symbol name
- that pair must be sufficient to resolve the description back to the target AST node
- real providers must only emit descriptions that still resolve to a live node
  in a managed workspace document

If a node should not be visible, do not emit a description for it. Do not emit
placeholder descriptions with missing ids.

## `ScopeProvider`

`ScopeProvider` exposes two operations:

- `getScopeEntry(...)` for exact lookup of the current reference text
- `visitScopeEntries(...)` to enumerate all visible candidates in lexical order

That order is important. Pegium visits nearer local scopes first, then outer
locals, then global exports.

## `References` contract

`References::findDeclarations(...)` and
`References::findDeclarationNodes(...)` return concrete matches only.

- the input CST node must already come from a managed workspace document
- declaration AST pointers are never null
- declaration CST nodes are always valid
- returned declarations always come from managed workspace documents

If there is no match, return an empty vector. Do not return placeholder or
partially resolved entries.

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
