# Formatter DSL

This page documents the user-facing formatting DSL built around
`pegium::lsp::AbstractFormatter`.

Use this page when you need the canonical formatter API surface rather than the
shorter task-oriented recipe.

## Creating a formatter

The minimal pattern is:

1. derive from `AbstractFormatter`
2. call the base constructor with `services`
3. register formatting methods in the constructor with `on<T>(...)`
4. assign an instance to `services->lsp.formatter`

Example:

```cpp
class MyFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit MyFormatter(const pegium::services::Services &services)
      : AbstractFormatter(services) {
    on<ast::Entity>(&MyFormatter::formatEntity);
    onHidden("ML_COMMENT", &MyFormatter::formatComment);
  }

protected:
  virtual void formatEntity(pegium::lsp::FormattingBuilder &builder,
                            const ast::Entity *entity) const;
  virtual void formatComment(HiddenNodeFormatter &comment) const;
};
```

Service wiring:

```cpp
services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
```

## Registration

Register one method per exact AST type:

```cpp
on<ast::Module>(&MyFormatter::formatModule);
on<ast::Entity>(&MyFormatter::formatEntity);
onHidden("ML_COMMENT", &MyFormatter::formatComment);
```

When the same AST type or hidden terminal rule is registered twice, the latest
registration overrides the previous one.

Most formatters should not override the global `format(...)` fallback. Register
typed methods instead and let the formatter engine dispatch them while walking
the AST.

## Main entry points

### `format(FormattingBuilder &, const AstNode *)`

Override this only if you need advanced fallback behavior. Most formatters
should rely on `on<T>(...)` and `onHidden(...)`.

### `FormattingBuilder::getNodeFormatter(const NodeT *node)`

Returns a node-scoped selector object for the supplied AST node.

## `NodeFormatter<T>` selectors

- `property<&T::member>()`
- `property<&T::vectorMember>(index)`
- `properties<&T::member...>()`
- `keyword("...")`
- `keyword("...", index)`
- `keywords("...", "...")`
- `hidden("RULE")`
- `hiddens("RULE")`
- `interior(start, end)`

All selections produce a `FormattingRegion`.

## `FormattingRegion`

- `prepend(action)`
- `append(action)`
- `surround(action)`
- `slice(start)`
- `slice(start, end)`

Selections are CST-backed and keep document order.

## Built-in actions

These helpers are protected members of `AbstractFormatter`:

- `noSpace`
- `oneSpace`
- `spaces(count)`
- `newLine`
- `newLines(count)`
- `indent`
- `noIndent`
- `fit(...)`

They can be used directly inside formatting methods:

```cpp
formatter.keyword("entity").append(oneSpace);
formatter.property<&ast::Field::name>().prepend(oneSpace);
```

## Hidden nodes

Hidden-node formatting is handled with `HiddenNodeFormatter`.

Useful operations:

- `node()`
- `ruleName()`
- `text()`
- `baseIndentation()`
- `indentation(delta)`
- `replace(text)`
- `prepend(action)`
- `append(action)`
- `surround(action)`

## Generic helpers

`AbstractFormatter` also provides higher-level helpers:

- `formatBlock(open, close, content, options)`
- `formatSeparatedList(separators, options)`
- `formatLineComment(text, options)`
- `formatLineComment(hiddenNode, options)`
- `formatMultilineComment(text, options)`

These helpers are designed to cover common patterns such as brace blocks,
comma-separated lists, and comment normalization.

## Related pages

- [Formatting](../recipes/custom-formatter.md)
- [Default LSP Services](lsp-services.md)
- [Semantic Model](semantic-model.md)
