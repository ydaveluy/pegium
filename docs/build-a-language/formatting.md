# Formatting

Formatting is implemented by deriving from `pegium::AbstractFormatter`.

The formatter lives in `services->lsp.formatter` and works on top of the CST.
That is why formatting is usually one of the last core features to add, after
the grammar and AST shape are already stable enough.

## Creating a formatter

Typical header:

```cpp
class DomainModelFormatter : public pegium::AbstractFormatter {
public:
  explicit DomainModelFormatter(const pegium::Services &services);

protected:
  virtual void formatEntity(pegium::FormattingBuilder &builder,
                            const ast::Entity *entity) const;
  virtual void formatLineComment(HiddenNodeFormatter &comment) const;
};
```

Typical implementation:

```cpp
DomainModelFormatter::DomainModelFormatter(
    const pegium::Services &services)
    : AbstractFormatter(services) {
  on<ast::Entity>(&DomainModelFormatter::formatEntity);
  onHidden("SL_COMMENT", &DomainModelFormatter::formatLineComment);
}
```

This constructor is where you register the formatting methods for the node types
and hidden tokens your language cares about.

## Formatting one node

Inside a formatting method, the usual flow is:

1. get a node-scoped formatter from the builder
2. select the CST-backed regions you care about
3. attach spacing, line-break, or indentation actions to those regions

Example:

```cpp
void DomainModelFormatter::formatEntity(
    pegium::FormattingBuilder &builder,
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

This is the basic Pegium formatting pattern: select a region, then describe the
layout you want around it.

## Registering several rules

The preferred style is one method per exact AST type:

```cpp
on<ast::DomainModel>(&MyFormatter::formatDomainModel);
on<ast::PackageDeclaration>(&MyFormatter::formatPackageDeclaration);
on<ast::Entity>(&MyFormatter::formatEntity);
on<ast::Feature>(&MyFormatter::formatFeature);
```

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

Inside `AbstractFormatter`, the main layout actions are:

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

Use them whenever the pattern is already standard in your language. That keeps
the formatter small and consistent.

## Formatting hidden nodes

Hidden nodes are handled through `HiddenNodeFormatter`.

Typical comment formatting method:

```cpp
void MyFormatter::formatLineComment(HiddenNodeFormatter &comment) const {
  comment.replace(AbstractFormatter::formatLineComment(comment));
}
```

This is the right place to normalize or reflow comment text when the layout of
the comment itself matters.

## Wiring the formatter into services

Creating the formatter class is not enough. You must also install it into the
language services:

```cpp
auto services = pegium::makeDefaultServices(
    sharedServices, "domain-model");

services->parser =
    std::make_unique<const domainmodel::parser::DomainModelParser>(*services);

services->lsp.formatter =
    std::make_unique<lsp::DomainModelFormatter>(*services);
```

Without the formatter assignment, formatting requests simply keep using the
empty default slot.

## Practical advice

- start with one or two important node types
- keep one formatting method per exact AST type
- prefer `formatBlock(...)` and `formatSeparatedList(...)` over repeating the
  same layout logic everywhere
- touch hidden-node formatting only when comment text itself must change

Use the [formatter DSL reference](../reference/formatter-dsl.md) for the full
API surface.
