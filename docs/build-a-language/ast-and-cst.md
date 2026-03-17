# AST and CST

Pegium keeps both the abstract syntax tree and the concrete syntax tree
available.

## AST

Define AST nodes by deriving from `pegium::AstNode`:

```cpp
struct Entity : pegium::AstNode {
  string name;
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};
```

The AST should model language semantics:

- names and scalar values
- containment between nodes
- cross-references
- optional and repeated properties

## AST field types

Pegium supports a small set of field shapes that cover the common language
modeling needs.

### Scalar values

Use regular value fields for textual or numeric data:

```cpp
struct NumberLiteral : pegium::AstNode {
  double value = 0.0;
};

struct Feature : pegium::AstNode {
  bool many = false;
  string name;
};
```

Typical scalar field types:

- `string`
- `bool`
- integer aliases inherited from `AstNode` such as `int32_t`, `uint64_t`, and
  so on
- floating-point values such as `double`
- enums
- custom value types, as long as your terminal or data-type rule can construct
  them
- `variant<T...>` when a property is intentionally one-of-several value shapes

Use scalar fields for values that are owned directly by the node and do not
represent containment or cross-document linking.

Example:

```cpp
struct Example : pegium::AstNode {
  variant<bool, string> value;
};
```

### Optional scalar values

Use `optional<T>` when the property may be absent:

```cpp
struct Test : pegium::AstNode {
  optional<string> testFile;
};
```

This is useful for optional names, strings, numbers, enums, and similar value
properties.

### Single contained child

Use `pointer<T>` for a single contained AST child:

```cpp
struct Evaluation : pegium::AstNode {
  pointer<Expression> expression;
};
```

`pointer<T>` is an alias for `std::unique_ptr<T>`. This is the standard way to
model containment in Pegium.

Typical use cases:

- one expression inside another node
- one optional-like child that is either present or absent
- one owned nested declaration

### Repeated contained children

Use `vector<pointer<T>>` for a list of contained AST children:

```cpp
struct Entity : pegium::AstNode {
  vector<pointer<Feature>> features;
};
```

This is the standard shape for repeated containment.

### Single reference

Use `reference<T>` for a link to another AST node:

```cpp
struct FunctionCall : pegium::AstNode {
  reference<AbstractDefinition> func;
};
```

A `reference<T>` stores reference text and resolves later through the linker. It
is not containment.

Typical use cases:

- super types
- called functions
- referenced states, commands, or environments

### Optional reference

Use `optional<reference<T>>` when a reference may be absent:

```cpp
struct Entity : pegium::AstNode {
  optional<reference<Entity>> superType;
};
```

### Repeated references

Use `vector<reference<T>>` when the source syntax contains several independent
references:

```cpp
struct Requirement : pegium::AstNode {
  vector<reference<Environment>> environments;
};
```

This is useful when the text contains several names that should each resolve to
one target.

### Multi-reference

Pegium also provides `multi_reference<T>` for one reference slot that may
resolve to several targets.

```cpp
struct Example : pegium::AstNode {
  multi_reference<MyNode> targets;
};
```

There is also `optional<multi_reference<T>>` when that slot itself is optional.

Use `multi_reference<T>` only when one reference occurrence in the source is
meant to resolve to multiple targets. If the source contains several explicit
names, `vector<reference<T>>` is usually the better shape.

### Repeated scalar values

Use `vector<T>` for repeated scalar data:

```cpp
struct Example : pegium::AstNode {
  vector<string> tags;
};
```

This is for repeated values, not repeated contained nodes. For repeated child
nodes, keep using `vector<pointer<T>>`.

## AST aliases inherited from `AstNode`

When you derive from `pegium::AstNode`, you can use these aliases directly in
the struct body:

- `string`
- `int8_t`, `int16_t`, `int32_t`, `int64_t`
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- `optional<T>`
- `variant<T...>`
- `vector<T>`
- `pointer<T>`
- `reference<T>`
- `multi_reference<T>`

That is why the examples can write `string`, `pointer<Expression>`, or
`vector<reference<Environment>>` without qualifying them.

## Recommended AST style

Prefer these shapes:

- scalar values for plain data
- `pointer<T>` for one contained child
- `vector<pointer<T>>` for repeated contained children
- `reference<T>` for one linked target
- `vector<reference<T>>` for repeated explicit links
- `optional<T>` only when absence is semantically meaningful

This matches the shipped examples and keeps the AST easy to traverse and link.

## CST

- `pegium::CstNodeView` represents a stable view into the parsed concrete tree
- offsets, ranges, children, siblings, hidden nodes, and recovered nodes are
  available through the CST API
- `pegium::CstUtils` contains lookup helpers for properties, keywords, interior
  nodes, and node-at-offset operations

## Why both matter

- AST drives semantics, validation, scoping, and most editor features
- CST is required for precise formatting, offset-based lookup, and source-level
  operations that must preserve text layout

## Recommended pattern

Model semantics in the AST first. Drop to CST only when the feature is
inherently source-aware, such as formatting, comment handling, or
cursor-position logic.
