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

So the semantic model in Pegium is not a post-processing artifact. It is the
combined result of AST node definitions and grammar wiring.

## Why this matters

The shape of the AST is not just an implementation detail. It directly affects:

- how references are represented and linked
- how validation reasons about the model
- how formatter and editor features find the right source regions
- how stable your language-specific code remains as the grammar evolves

## References are part of the model

References are not just strings stored in fields. They carry:

- the text written by the user
- the owning container, feature name, target type, and reference cardinality
- the source CST node, when available
- the resolved target, once linking has happened

That is why the same model can support linking, completion, rename, and hover
without inventing separate data structures for each feature.

## Runtime type information

Runtime AST reflection also treats `pegium::AstNode` as the implicit root type
of every registered AST class. In practice, `AstReflection::isSubtype(...)` and
`AstReflection::getAllSubTypes(...)` stay consistent with
`AstReflection::isInstance(..., typeid(pegium::AstNode))`.

`AstReflection::getAllTypes()` and `AstReflection::getAllSubTypes(...)` expose
stable `std::unordered_set` views over the bootstrapped registry state, so
their iteration order is intentionally unspecified.

Pegium bootstraps that reflection once during single-threaded language
registration by walking the parser entry rule. Because this bootstrap probes
the participating AST types directly, AST-producing parser aliases such as
`Rule<T>` and `Infix<T, ...>` require `DefaultConstructibleAstNode<T>`.

That constraint only applies to AST types that are produced directly by parser
rules. Abstract or non-default-constructible AST supertypes can still be used
as reference targets or as containment slot supertypes in assignments; the
bootstrap only needs their runtime type information for subtype filtering.

Pegium therefore treats the parsed AST as a mutable technical model of the
source program. The recommended pattern is:

- keep parser-managed AST nodes simple and default-constructible
- express semantic constraints with validation, linking, or later transforms
- build a stricter domain model after parsing if your application needs one

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
than just whatever happened to fall out of the first grammar draft.

Good signs of a stable model:

- containment is explicit
- references are modeled as references, not as raw strings
- optionality reflects real semantics
- later services can reason about the tree without special-case hacks

## Related pages

- [Reference](index.md)
- [Resolve Cross-References](../learn/workflow/resolve_cross_references.md)
- [Glossary](glossary.md)
