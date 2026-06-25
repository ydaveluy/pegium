# Define the AST

Your AST is the semantic model of a document: ordinary C++ structs that the grammar fills. **Define these types first** — the grammar and every service (scoping, validation, formatting, editor features) are built around them.

Pegium also keeps a CST (concrete syntax tree) for the exact source structure; reach for it only when a feature needs the literal text (see [The CST is about source structure](#the-cst-is-about-source-structure)).

## `AstNode` vs `NamedAstNode`

Every AST type derives from `pegium::AstNode`. A declaration that introduces a *name* — so it can be referenced, exported to the global scope, and linked — should derive from `pegium::NamedAstNode`, which adds the `name` field:

```cpp
struct Feature : pegium::NamedAstNode {       // has a name
  bool many = false;
  reference<Type> type;
};

struct Entity : pegium::NamedAstNode {        // has a name
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};

struct DomainModel : pegium::AstNode {        // a container with no name of its own
  vector<pointer<AbstractElement>> elements;
};
```

`NamedAstNode` supplies the `name` field that the default name provider reads directly — and it is the **only** thing that provider names. A plain `AstNode` is unnamed even if its grammar assigns a `name` field: it is not exported, scoped, or linked by name. Derive `NamedAstNode` for anything with a name; if a named type genuinely cannot derive it, override the `NameProvider` service (see [Custom scope provider](../recipes/custom-scope-provider.md)).

Build a deeper hierarchy by deriving your own intermediate base types (`struct Type : AbstractElement {};`). To also *reuse and extend the grammar rules* of a base language, see [Extend a base grammar with `super()`](grammar.md#extend-a-base-grammar-with-super).

## Field types

Inside an AST struct these aliases are available unqualified — they are inherited from `AstNode`:

| Field shape | Write it as | Use it for |
| --- | --- | --- |
| Scalar | `bool`, `char`, `int8_t`…`int64_t`, `uint8_t`…`uint64_t`, `float`, `double`, `string`, any `enum` | plain values: flags, numbers, names, an enum keyword choice |
| Contained child | `pointer<T>` | one owned child node (`T` derives from `AstNode`) |
| Repeated children | `vector<pointer<T>>` | a list of owned child nodes |
| Repeated scalars | `vector<Scalar>` | a list of plain values |
| Cross-reference | `reference<T>` | one link to another (possibly cross-document) node, resolved after parsing |
| Multi-target reference | `multi_reference<T>` | one reference occurrence that resolves to several targets |
| Several references | `vector<reference<T>>` | several independent links written in the source |
| Optional | `optional<T>` | a field whose absence is meaningful |
| Alternatives | `variant<A, B, …>` | a field that holds one of several shapes |

A few rules of thumb:

- **Scalars** are also the value type of `Terminal<T>` / `Rule<T>` (e.g. `Terminal<int32_t>`, `Terminal<double>`, `Terminal<MyEnum>`). The accepted scalar value types are `bool`, `char`, the fixed-width signed/unsigned integers, floating point, enums, and `string`.
- **`pointer<T>`** is a non-owning raw pointer to an arena-owned child (use it, not a value, for nested nodes). It is `nullptr` when the grammar did not assign it — for example an optional child — so null-check a single `pointer<T>` field unless the rule always fills it. Children reached through `vector<pointer<T>>` or `getContent()` are never null.
- **`reference<T>`** resolves lazily on first dereference — check it converts to `true` (or that the document has no error diagnostics) before following it. Use `multi_reference<T>` only when a single occurrence legitimately resolves to several targets.
- **`optional<T>`** only when absence is part of the language semantics, not merely convenient.

Keeping to this small set of shapes keeps the model predictable for both your code and Pegium.

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
