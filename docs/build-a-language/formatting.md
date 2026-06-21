# Formatting

Lay out your language's source code by deriving from `pegium::AbstractFormatter`. The formatter lives in `services->lsp.formatter` and works on top of the CST, so it's usually one of the last core features you add — after the grammar and AST shape are stable.

For a task-oriented checklist, see [Recipes — Formatting](../recipes/custom-formatter.md). For the full API surface, see the [Formatter DSL reference](../reference/formatter-dsl.md).

## Creating a formatter

A typical header:

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

The constructor registers the formatting methods for the node types and hidden tokens your language cares about:

```cpp
DomainModelFormatter::DomainModelFormatter(
    const pegium::Services &services)
    : AbstractFormatter(services) {
  on<ast::Entity>(&DomainModelFormatter::formatEntity);
  onHidden("SL_COMMENT", &DomainModelFormatter::formatLineComment);
}
```

## Formatting one node

Inside a formatting method:

1. get a node-scoped formatter from the builder
2. select the CST-backed regions you care about
3. attach spacing, line-break, or indentation actions to those regions

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

The pattern: select a region, then describe the layout you want around it.

## Registering several rules

Use one method per exact AST type:

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

`builder.getNodeFormatter(node)` returns a `NodeFormatter<T>` scoped to the CST subtree of that AST node. Common selections:

- `property<&T::member>()`
- `property<&T::vectorMember>(index)`
- `properties<&T::member...>()`
- `keyword("...")`
- `keywords("...", "...")`
- `hidden("RULE")`
- `hiddens("RULE")`
- `interior(start, end)`

```cpp
auto formatter = builder.getNodeFormatter(feature);
formatter.keyword(":").prepend(noSpace).append(oneSpace);
```

## Built-in actions

`AbstractFormatter` exposes these layout actions as protected members:

- `noSpace`
- `oneSpace`
- `spaces(count)`
- `newLine`
- `newLines(count)`
- `indent`
- `noIndent`
- `fit(...)`

Use them directly inside your formatter methods:

```cpp
formatter.keyword("entity").append(oneSpace);
formatter.keyword(":").prepend(noSpace).append(oneSpace);
```

## Generic helpers

`AbstractFormatter` also provides higher-level helpers for recurring layout patterns:

- `formatBlock(...)`
- `formatSeparatedList(...)`
- `formatLineComment(...)`
- `formatMultilineComment(...)`

Reach for them whenever the pattern is standard in your language. They keep the formatter small and consistent.

## Formatting hidden nodes

Hidden nodes go through `HiddenNodeFormatter`. Use this to normalize or reflow comment text when the comment's own layout matters:

```cpp
void MyFormatter::formatLineComment(HiddenNodeFormatter &comment) const {
  comment.replace(AbstractFormatter::formatLineComment(comment));
}
```

## Wiring the formatter into services

Creating the formatter class isn't enough — install it into the language services:

```cpp
// lsp/Module.cpp — the LSP install module assigns the formatter slot:
void installDomainModelLspModule(DomainModelServices &services) {
  services.lsp.formatter = std::make_unique<DomainModelFormatter>(services);
}

// createDomainModelServices builds the container and runs the install modules:
auto services = pegium::makeDefaultServices<DomainModelServices>(
    sharedServices, "domain-model");
installDomainModelCoreModule(*services);
installDomainModelLspModule(*services);
```

Without the assignment, formatting requests keep using the empty default slot.

## Practical advice

- start with one or two important node types
- keep one formatting method per exact AST type
- prefer `formatBlock(...)` and `formatSeparatedList(...)` over repeating the same layout logic everywhere
- touch hidden-node formatting only when comment text itself must change

## Related pages

- [Recipes — Formatting](../recipes/custom-formatter.md)
- [Formatter DSL reference](../reference/formatter-dsl.md)
