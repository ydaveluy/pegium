# Formatter DSL

The user-facing formatting DSL is built around `pegium::AbstractFormatter`. This page is the canonical API surface; it complements the task-oriented formatting guide.

## Creating a formatter

To build a formatter:

1. derive from `AbstractFormatter`
2. call the base constructor with `services`
3. register formatting methods in the constructor with `on<T>(...)`
4. assign an instance to `services->lsp.formatter`

```cpp
class MyFormatter : public pegium::AbstractFormatter {
public:
  explicit MyFormatter(const pegium::Services &services)
      : AbstractFormatter(services) {
    on<ast::Entity>(&MyFormatter::formatEntity);
    onHidden("ML_COMMENT", &MyFormatter::formatComment);
  }

protected:
  virtual void formatEntity(pegium::FormattingBuilder &builder,
                            const ast::Entity *entity) const;
  virtual void formatComment(HiddenNodeFormatter &comment) const;
};
```

Wire it into the services:

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

Registering the same AST type or hidden terminal rule twice overrides the previous registration.

Don't override the global `format(...)` fallback. Register typed methods and let the formatter engine dispatch them as it walks the AST.

## Main entry points

### `format(FormattingBuilder &, const AstNode *)`

Override this only for advanced fallback behavior.

### `FormattingBuilder::getNodeFormatter(const NodeT *node)`

Returns a node-scoped selector for the supplied AST node.

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

Every selection produces a `FormattingRegion`.

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

Use them directly inside formatting methods:

```cpp
formatter.keyword("entity").append(oneSpace);
formatter.property<&ast::Field::name>().prepend(oneSpace);
```

## Hidden nodes

Format hidden nodes with `HiddenNodeFormatter`:

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

`AbstractFormatter` provides higher-level helpers for common patterns such as brace blocks, comma-separated lists, and comment normalization:

- `formatBlock(open, close, content, options)`
- `formatSeparatedList(separators, options)`
- `formatLineComment(text, options)`
- `formatLineComment(hiddenNode, options)`
- `formatMultilineComment(text, options)`
- `formatMultilineComment(hiddenNode, options)`

## Related pages

- [Formatting](../recipes/custom-formatter.md)
- [LSP Services](../build-a-language/lsp-services.md)
- [Semantic Model](semantic-model.md)
