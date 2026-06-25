# Custom LSP Features

Swap individual LSP providers without rewriting the whole language server layer.

## Common extension points

- completion
- hover
- rename
- references
- semantic tokens
- formatter

## Strategy

Start from the default services, then swap one provider at a time. The rest of the language server stack stays stable while you iterate on a single feature.

Typical wiring:

```cpp
auto services = pegium::makeDefaultServices(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
services->lsp.hoverProvider = std::make_unique<lsp::MyHoverProvider>(*services);
```

For completion, subclass `pegium::DefaultCompletionProvider` and override its narrow hooks rather than rewriting the whole provider. The [completion provider reference](../reference/completion-provider.md) lists the available extension points.

## When to write a custom provider

- the language has unusual scoping or symbol lookup rules
- completion items depend on semantic state the default provider cannot see
- hover content needs custom documentation rendering
- rename or definition behavior depends on domain-specific semantics

## Practical advice

Replace the most visible or most language-specific feature first:

1. formatter
2. hover
3. completion
4. navigation features such as definition or rename
