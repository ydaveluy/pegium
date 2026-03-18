# 4. Shape the AST and CST

After defining the grammar, you already have a parser. But a language project
needs more than “accepted text”. It needs a semantic model that later services
can rely on.

In Pegium, this step is not a separate code generation phase. AST and CST
shape are defined directly by the parser and the C++ node types you choose.

## The semantic model

The AST is the semantic tree of your language. A typical AST node looks like
this:

```cpp
struct Entity : pegium::AstNode {
  string name;
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};
```

This one node already captures three different semantics:

- `name` is plain scalar data
- `superType` is a reference that will resolve later
- `features` are contained child nodes owned by the entity

## How the parser builds the model

The grammar determines how those fields are populated. For example:

```cpp
Rule<ast::Entity> EntityRule{
    "Entity",
    "entity"_kw.i() + assign<&ast::Entity::name>(ID) +
        option("extends"_kw.i() +
               assign<&ast::Entity::superType>(QualifiedName)) +
        "{"_kw + many(append<&ast::Entity::features>(FeatureRule)) +
        "}"_kw};
```

This is where parsing and model construction meet:

- `assign` writes one value into a field
- `append` adds repeated children or values
- `create`, `action`, and `nest` help shape more advanced trees

## Why the CST matters too

Pegium keeps the CST alongside the AST. This is important because not every
feature is purely semantic.

The CST is later used for:

- formatting
- comment handling
- precise keyword and property lookup
- cursor-position-sensitive editor features

So even when the AST is your main semantic model, the CST remains a first-class
part of the language infrastructure.

## Recommended modeling style

When shaping the AST, keep it close to the semantics of the language:

- use scalar fields for plain values
- use `pointer<T>` for owned children
- use `vector<pointer<T>>` for repeated owned children
- use `reference<T>` for links to other nodes
- use `optional<T>` only when absence is meaningful

This tends to make validation, linking, and formatting easier later.

## What to expect at the end of this step

At the end of this step, your parser should produce the node structure that the
rest of the framework will work with, and that structure should already feel
like the language model you want to reason about.

## Continue with

- [Semantic Model](../../reference/semantic-model.md)
- [Glossary](../../reference/glossary.md)
- [5. Resolve Cross-References](resolve_cross_references.md)
