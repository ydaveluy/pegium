# References and Scoping

References turn a parsed tree into a usable language model: a written name points to a real declaration, possibly in another file. Pegium splits that job into small services so linking, completion, rename, and navigation reuse the same information. Once you know which lever to pull, the hands-on recipes live in [Qualified Names](../recipes/scoping/qualified-names.md) and [Custom Scope Provider](../recipes/custom-scope-provider.md).

## The main roles

These are separate on purpose. Most languages customize only one or two.

- `NameProvider` decides how declarations are named.
- `ScopeComputation` decides what a document exports and which local symbols are precomputed.
- `ScopeProvider` decides what is visible from one concrete reference site.
- `Linker` resolves the written reference text to one target.

## Model references explicitly in the AST

Make a cross-reference a reference field, not just a string:

```cpp
struct Feature : pegium::NamedAstNode {
  reference<Type> type;
};
```

Parsing records the written text; the linker later resolves it to a real target node. The key mental model:

- parsing records what the user wrote
- linking decides what it refers to

## Start with the default naming pattern

If your declarations all have a `name`, derive them from `pegium::NamedAstNode`:

```cpp
struct Entity : pegium::NamedAstNode {};
struct DataType : pegium::NamedAstNode {};
struct PackageDeclaration : pegium::NamedAstNode {};
```

That keeps naming in the AST and works well with the default naming service. For many languages this is already the right pattern.

## Exported symbols vs visible symbols

Two questions are easy to mix up:

- what does this document contribute to the workspace? (exported symbols)
- what may this specific reference see right here? (visible symbols)

So debug scoping problems in this order:

1. confirm the declaration is exported with the right name
2. confirm the current scope contains the right candidates
3. only then question the linker

## Most customizations start in scope computation

The default linker and scope provider are often enough. Your first real customization is usually `ScopeComputation`, where you implement:

- qualified names
- package-like nesting
- imported symbols added to local scopes
- precomputed local visibility rules

It is also a good performance trade-off: the work happens once per document rebuild instead of on every completion or linking request.

## Customize the scope provider only when visibility depends on context

Override `ScopeProvider` when lookup depends on the reference site in a way precomputed symbols cannot express cleanly:

- visibility that depends on imports or modifiers
- context-sensitive filtering
- rules that differ significantly across reference sites

Keep the linker default unless the resolution policy itself needs to change.

## What this unlocks

With correct references and scopes, much of the editor experience works from the same model:

- go to definition
- find references
- rename
- completion on references
- diagnostics for unresolved names

## Start from examples

- `domainmodel` — best starting point for nested declarations and qualified names
- `requirements` — best starting point for cross-file and multi-language scenarios

## Related pages

- [Build a Language End-to-End](../learn/walkthrough.md)
- [Qualified Names](../recipes/scoping/qualified-names.md)
- [Custom Scope Provider](../recipes/custom-scope-provider.md)
