# Formatting

Formatting is implemented by deriving from `pegium::lsp::AbstractFormatter`.

## Minimal formatter shape

A formatter is a regular class stored in `services->lsp.formatter`.

Typical header:

```cpp
class DomainModelFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit DomainModelFormatter(const pegium::services::Services &services);

protected:
  virtual void formatEntity(pegium::lsp::FormattingBuilder &builder,
                            const ast::Entity *entity) const;
  virtual void formatLineComment(HiddenNodeFormatter &comment) const;
};
```

Typical implementation:

```cpp
DomainModelFormatter::DomainModelFormatter(
    const pegium::services::Services &services)
    : AbstractFormatter(services) {
  on<ast::Entity>(&DomainModelFormatter::formatEntity);
  onHidden("SL_COMMENT", &DomainModelFormatter::formatLineComment);
}
```

That constructor is where you declare which formatting method should run for
each exact AST type or hidden terminal rule.

## First formatting rule

Inside a formatting method:

1. get a node-scoped formatter from the builder
2. select CST-backed regions
3. attach whitespace actions to those regions

Example:

```cpp
void DomainModelFormatter::formatEntity(
    pegium::lsp::FormattingBuilder &builder,
    const ast::Entity *entity) const {
  auto formatter = builder.getNodeFormatter(entity);
  formatter.keyword("entity").append(oneSpace);

  if (entity->superType.has_value()) {
    formatter.keyword("extends").prepend(oneSpace).append(oneSpace);
  }

  const auto openBrace = formatter.keyword("{");
  const auto closeBrace = formatter.keyword("}");
  formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
}
```

This does three things:

- forces one space after `entity`
- normalizes spacing around `extends`
- formats the `{ ... }` block with the generic block helper

## Registering several rules

The preferred style is one method per exact AST type:

```cpp
on<ast::DomainModel>(&MyFormatter::formatDomainModel);
on<ast::PackageDeclaration>(&MyFormatter::formatPackageDeclaration);
on<ast::Entity>(&MyFormatter::formatEntity);
on<ast::Feature>(&MyFormatter::formatFeature);
```

The formatter engine walks the AST and dispatches to the registered method when
it encounters that exact node type.

Use `onHidden("RULE_NAME", ...)` for hidden tokens such as comments:

```cpp
onHidden("ML_COMMENT", &MyFormatter::formatComment);
onHidden("SL_COMMENT", &MyFormatter::formatLineComment);
```

## Selecting regions

`builder.getNodeFormatter(node)` returns a `NodeFormatter<T>` scoped to the CST
subtree of that AST node.

Common selections:

- `property<&T::member>()`
- `property<&T::vectorMember>(index)`
- `properties<&T::member...>()`
- `keyword("...")`
- `keywords("...", "...")`
- `hidden("RULE")`
- `hiddens("RULE")`
- `interior(start, end)`

Example:

```cpp
auto formatter = builder.getNodeFormatter(feature);
formatter.keyword(":").prepend(noSpace).append(oneSpace);
```

## Built-in actions

Inside `AbstractFormatter`, the main whitespace actions are:

- `noSpace`
- `oneSpace`
- `spaces(count)`
- `newLine`
- `newLines(count)`
- `indent`
- `noIndent`
- `fit(...)`

These are protected members, so they can be used directly inside your
formatter methods:

```cpp
formatter.keyword("entity").append(oneSpace);
formatter.keyword(":").prepend(noSpace).append(oneSpace);
```

## Generic helpers

`AbstractFormatter` also provides higher-level helpers for recurring layout
patterns:

- `formatBlock(...)`
- `formatSeparatedList(...)`
- `formatLineComment(...)`
- `formatMultilineComment(...)`

Use them whenever the rule is a standard block, comma-separated list, or
comment normalization. That keeps the formatter small and consistent.

## Formatting hidden nodes

Hidden nodes are handled through `HiddenNodeFormatter`.

Typical comment formatting method:

```cpp
void MyFormatter::formatLineComment(HiddenNodeFormatter &comment) const {
  comment.replace(AbstractFormatter::formatLineComment(comment));
}
```

This is the right place to:

- normalize line comments
- reflow multiline comments
- keep documentation tags such as `@param ...` consistent

## Wiring the formatter into services

Creating the formatter class is not enough. You must also install it into the
language services:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "domain-model", std::move(parser));

services->lsp.formatter =
    std::make_unique<lsp::DomainModelFormatter>(*services);
```

Without this assignment, the language server keeps the default formatter slot
empty and formatting requests do nothing.

## Practical advice

- start with one or two node types
- keep one formatting method per exact AST type
- prefer `formatBlock(...)` and `formatSeparatedList(...)` over repeating the
  same brace or comma logic everywhere
- use hidden-node formatting only when comment text itself needs rewriting

Use the [formatter DSL reference](../reference/formatter-dsl.md) for the full
API surface.
