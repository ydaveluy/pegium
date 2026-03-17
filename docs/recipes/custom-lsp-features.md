# Custom LSP Features

Pegium lets you replace individual LSP providers without rewriting the whole
language server layer.

## Common extension points

- completion
- hover
- rename
- references
- semantic tokens
- formatter

## Strategy

Start from the default services, then swap one provider at a time. This keeps
the rest of the language server stack stable while you iterate on a specific
feature.

Typical wiring:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language", std::move(parser));

services->lsp.hoverProvider = std::make_unique<lsp::MyHoverProvider>(*services);
```

For completion-specific customization, prefer subclassing
`lsp::DefaultCompletionProvider` and overriding its narrow hooks instead of
rewriting the whole provider. See the
[completion provider reference](../reference/completion-provider.md) for the
available extension points.

## When to write a custom provider

- the language has unusual scoping or symbol lookup rules
- completion items depend on semantic state that the default provider cannot see
- hover content needs custom documentation rendering
- rename or definition behavior depends on domain-specific semantics

## Recommended order

Replace the most visible or most language-specific feature first:

1. formatter
2. hover
3. completion
4. navigation features such as definition or rename
