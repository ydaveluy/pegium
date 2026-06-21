# AST and CST

Pegium keeps two complementary views of a document:

- the AST for language semantics
- the CST for source structure

Start with the AST. Reach for the CST once your tooling needs the exact source.

## Treat the AST as your language model

AST nodes are ordinary C++ types derived from `pegium::AstNode`. Declarations with a name derive from `pegium::NamedAstNode`, which supplies the `name` field so the default naming and linking services pick them up:

```cpp
struct Entity : pegium::NamedAstNode {
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};
```

That one type already says a lot:

- `name` (from `NamedAstNode`) is the declared symbol name
- `superType` is a link to another declaration
- `features` are owned children

A well-shaped AST makes validation, scoping, navigation, and formatting easier later.

## Use a small set of field shapes

Most languages need only a few recurring patterns.

- Use scalar values for names, numbers, booleans, enums, and other plain data.
- Use `pointer<T>` for one contained child.
- Use `vector<pointer<T>>` for repeated contained children.
- Use `reference<T>` for one link to another node.
- Use `vector<reference<T>>` for several independent links written in the source.
- Use `multi_reference<T>` only when one reference occurrence resolves to several targets.
- Use `optional<T>` only when absence is part of the language semantics.

This keeps the model predictable for both users of the AST and Pegium itself.

## Keep parser-managed nodes simple

Pegium parses directly into your AST types, so parser-created nodes should stay lightweight and default-constructible.

- Keep parser-managed nodes as the technical model of the source text.
- Enforce semantic rules in validation and linking.
- Build a stricter application model afterwards only if you need one.

This avoids fighting the parser while keeping your domain logic clean.

## The CST is about source structure

The CST preserves what the user actually wrote:

- token boundaries
- hidden nodes such as comments
- exact keyword positions
- recovered structure after syntax errors

That makes CST access matter for:

- formatting
- hover over comments
- precise selections and ranges
- cursor-sensitive editor features

## Use the AST first, then drop to the CST when needed

- If the feature is semantic, start from the AST.
- If the feature depends on exact text layout, use the CST.

For example:

- validation and scoping are AST-first
- formatting and comment-aware features are CST-aware

## Recommended modeling checklist

When a model starts feeling awkward, check these points:

1. Are contained children modeled as containment rather than plain values?
2. Are cross-references modeled as `reference<T>` rather than raw strings?
3. Is optionality meaningful, or just convenient?
4. Does the AST feel natural to traverse without special-case helpers?

## Related pages

- [Build a Language End-to-End](../learn/walkthrough.md)
- [Semantic Model](../reference/semantic-model.md)
- [References and Scoping](references-and-scoping.md)
