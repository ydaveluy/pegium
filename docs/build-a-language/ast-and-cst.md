# AST and CST

Pegium keeps two complementary views of a document:

- the AST for language semantics
- the CST for source structure

Most language work starts with the AST. Most source-aware tooling eventually
needs the CST too.

## Treat the AST as your language model

AST nodes are ordinary C++ types derived from `pegium::AstNode`:

```cpp
struct Entity : pegium::AstNode {
  string name;
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};
```

That one type already says a lot:

- `name` is plain data
- `superType` is a link to another declaration
- `features` are owned children

If the AST is shaped well, validation, scoping, navigation, and formatting all
become easier later.

## Use a small set of field shapes

Most languages only need a few recurring patterns.

- Use scalar values for names, numbers, booleans, enums, and other plain data.
- Use `pointer<T>` for one contained child.
- Use `vector<pointer<T>>` for repeated contained children.
- Use `reference<T>` for one link to another node.
- Use `vector<reference<T>>` for several independent links written in the
  source.
- Use `multi_reference<T>` only when one reference occurrence is expected to
  resolve to several targets.
- Use `optional<T>` only when absence is part of the language semantics.

This keeps the model predictable for both users of the AST and Pegium itself.

## Keep parser-managed nodes simple

Pegium parses directly into your AST types. That means AST nodes created by the
parser should stay lightweight and default-constructible.

A good rule of thumb is:

- keep parser-managed nodes as the technical model of the source text
- enforce semantic rules in validation and linking
- build a stricter application model afterwards only if you really need one

This avoids fighting the parser while still keeping your domain logic clean.

## The CST is about source structure

The CST preserves what the user actually wrote:

- token boundaries
- hidden nodes such as comments
- exact keyword positions
- recovered structure after syntax errors

That is why CST access matters for:

- formatting
- hover over comments
- precise selections and ranges
- cursor-sensitive editor features

## Use the AST first, then drop to the CST when needed

A useful decision rule is simple:

- if the feature is semantic, start from the AST
- if the feature depends on exact text layout, use the CST

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

- [4. Shape the AST and CST](../learn/workflow/generate_ast.md)
- [Semantic Model](../reference/semantic-model.md)
- [References and Scoping](references-and-scoping.md)
