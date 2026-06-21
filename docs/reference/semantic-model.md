# Semantic Model

Your semantic model is the AST that parsing builds. In Pegium, two things shape it directly:

- the C++ AST node types you define
- the grammar assignments and actions that populate them

Pegium does not infer a separate semantic type system from another DSL. The model is already present in your C++ types.

## AST fields shape the model

Consider this type:

```cpp
struct Entity : pegium::NamedAstNode {
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};
```

It already tells the framework a lot:

- `name` (inherited from `NamedAstNode`) is the declared symbol name
- `superType` is a link to another node
- `features` are owned nested children

The parser decides how those fields are populated:

```cpp
Rule<ast::Entity> EntityRule{
    "Entity",
    "entity"_kw.i() + assign<&ast::Entity::name>(ID) +
        option("extends"_kw.i() +
               assign<&ast::Entity::superType>(QualifiedName)) +
        "{"_kw + many(append<&ast::Entity::features>(FeatureRule)) +
        "}"_kw};
```

The semantic model is the combined result of AST definitions and grammar wiring.

The AST shape is not just an implementation detail. It affects:

- how references are represented and linked
- how validation reasons about the model
- how the formatter and editor features find source regions
- how stable your language code stays as the grammar evolves

## References are part of the model

References are more than strings. They let one model support linking, completion, rename, and hover without separate data structures per feature.

## Runtime type information

Pegium keeps runtime type information for registered AST classes. The consequence for you: AST types produced directly by parser rules should stay default-constructible and lightweight.

The recommended pattern:

- keep parser-managed AST nodes simple
- express semantic constraints with validation and linking
- build a stricter application model afterwards only if needed

## Why the CST still matters

The AST is the semantic model, but Pegium keeps the CST alongside it because many editor features still need source structure:

- formatting
- comment rewriting
- keyword lookup
- precise property ranges
- cursor-sensitive features

So the working model of a Pegium language is AST plus CST plus references.

## Practical advice

For mature languages, treat the AST as a deliberate API rather than whatever fell out of the first grammar draft.

A stable model has:

- explicit containment
- references modeled as references, not raw strings
- optionality that reflects real semantics
- a tree later services can reason about without special-case hacks

## Related pages

- [Reference](index.md)
- [References and Scoping](../build-a-language/references-and-scoping.md)
- [Glossary](glossary.md)
