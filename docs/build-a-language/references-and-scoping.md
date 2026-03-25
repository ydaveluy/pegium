# References and Scoping

References are what turn a parsed tree into a usable language model. They let a
written name point to a real declaration, possibly in another file.

Pegium splits that job into a few small services so the same information can be
reused by linking, completion, rename, and navigation.

## The main roles

- `NameProvider` decides how declarations are named.
- `ScopeComputation` decides what symbols a document exports and what local
  symbols should be precomputed.
- `ScopeProvider` decides what is visible from one concrete reference site.
- `Linker` resolves the written reference text to one target.

These are separate on purpose. Most languages only need to customize one or two
of them.

## Model references explicitly in the AST

A cross-reference should be a reference field in the AST, not just a string:

```cpp
struct Feature : pegium::AstNode {
  string name;
  reference<Type> type;
};
```

During parsing, Pegium records the written text. Later, the linker resolves that
text to a real target node.

This is the key mental model:

- parsing records what the user wrote
- linking decides what it refers to

## Recommended naming pattern

For many languages, the default naming pattern is already the right one.

## Start with the default naming pattern

If your declarations all have a `name`, the easiest setup is to derive them
from `pegium::NamedAstNode`:

```cpp
struct Entity : pegium::NamedAstNode {};
struct DataType : pegium::NamedAstNode {};
struct PackageDeclaration : pegium::NamedAstNode {};
```

That keeps naming in the AST, where it usually belongs, and works well with the
default naming service.

## Exported symbols and visible symbols are different

Two questions are easy to mix up:

- what does this document contribute to the workspace?
- what may this specific reference see right here?

The first question is about exported symbols. The second is about visible
symbols.

This is why many scoping problems are best debugged in this order:

1. make sure the declaration is exported with the right name
2. make sure the current scope contains the right candidates
3. only then question the linker

## Most customizations start in scope computation

In many projects, the default linker and default scope provider are already
enough. The first real customization is usually `ScopeComputation`.

That is where you typically implement:

- qualified names
- package-like nesting
- imported symbols added to local scopes
- precomputed local visibility rules

This is also a good performance trade-off, because the work happens once per
document rebuild instead of on every completion or linking request.

## Customize the scope provider only when visibility depends on context

Override `ScopeProvider` when lookup itself depends on the reference site in a
way that precomputed symbols cannot express cleanly.

Typical cases:

- visibility that depends on imports or modifiers
- context-sensitive filtering
- rules that differ significantly across reference sites

Keep the linker default unless the resolution policy itself needs to change.

## What this unlocks

Once references and scopes are correct, a large part of the editor experience
starts working from the same model:

- go to definition
- find references
- rename
- completion on references
- diagnostics for unresolved names

## Start from examples

- `domainmodel` is the best starting point for nested declarations and
  qualified names
- `requirements` is the best starting point for cross-file and multi-language
  scenarios

## Related pages

- [5. Resolve Cross-References](../learn/workflow/resolve_cross_references.md)
- [Qualified Names](../recipes/scoping/qualified-names.md)
- [Custom Scope Provider](../recipes/custom-scope-provider.md)
