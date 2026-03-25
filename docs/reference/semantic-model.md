# Semantic Model

When AST nodes are created during parsing, they become the semantic model of
your language. In Pegium, that semantic model is shaped directly by two things:

- the C++ AST node types you define
- the grammar assignments and actions that populate them

Unlike generator-centric workflows, Pegium does not infer a separate semantic
type system from another DSL. The semantic model is already present in your C++
types.

This page explains how AST types, grammar assignments, references, and CST
structure fit together.

## AST fields shape the model

Consider this example:

```cpp
struct Entity : pegium::AstNode {
  string name;
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};
```

This one type already tells the framework a lot:

- `name` is plain scalar semantic data
- `superType` is a link to another node
- `features` are owned nested children

The parser then decides how those fields are populated:

```cpp
Rule<ast::Entity> EntityRule{
    "Entity",
    "entity"_kw.i() + assign<&ast::Entity::name>(ID) +
        option("extends"_kw.i() +
               assign<&ast::Entity::superType>(QualifiedName)) +
        "{"_kw + many(append<&ast::Entity::features>(FeatureRule)) +
        "}"_kw};
```

So the semantic model in Pegium is the combined result of AST node definitions
and grammar wiring.

## Why this matters

The shape of the AST is not just an implementation detail. It directly affects:

- how references are represented and linked
- how validation reasons about the model
- how formatter and editor features find the right source regions
- how stable your language-specific code remains as the grammar evolves

## References are part of the model

References are more than strings. They let the same model support linking,
completion, rename, and hover without inventing separate data structures for
each feature.

## Runtime type information

Pegium also keeps runtime type information for registered AST classes. For
language authors, the practical consequence is simple: AST types produced
directly by parser rules should stay default-constructible and lightweight.

The recommended pattern is:

- keep parser-managed AST nodes simple
- express semantic constraints with validation and linking
- build a stricter application model afterwards only if needed

## Why the CST still matters

The AST is the semantic model, but Pegium keeps the CST alongside it because
many editor-facing features still need source structure:

- formatting
- comment rewriting
- keyword lookup
- precise property ranges
- cursor-sensitive features

So the real working model of a Pegium language is AST plus CST plus references.

## Stable modeling advice

For mature languages, it is worth treating the AST as a deliberate API rather
than whatever happened to fall out of the first grammar draft.

Good signs of a stable model are:

- containment is explicit
- references are modeled as references, not raw strings
- optionality reflects real semantics
- later services can reason about the tree without special-case hacks

## Related pages

- [Reference](index.md)
- [Resolve Cross-References](../learn/workflow/resolve_cross_references.md)
- [Glossary](glossary.md)
