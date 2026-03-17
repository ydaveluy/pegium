# LSP Services

Pegium exposes editor features through `pegium::services::Services`.

## Service layout

`Services` combines:

- core language services
- LSP feature services under `services.lsp`
- access to shared workspace services

This means one language service object gives you both the semantic layer and
the editor layer.

## Default providers

Pegium already ships defaults for many features:

- completion
- hover
- document symbols
- references
- definition and type definition
- rename
- folding ranges
- selection ranges
- formatter
- semantic tokens
- call hierarchy and type hierarchy

Not every language needs all of them immediately. The useful part is that the
slots already exist and can be filled or replaced incrementally.

## Customization strategy

Start from `makeDefaultServices(...)`, then replace only the providers that are
language-specific. This keeps the baseline behavior while letting you override
completion, formatting, hover, or navigation logic where needed.

Typical setup:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language", std::move(parser));

services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
services->lsp.hoverProvider = std::make_unique<lsp::MyHoverProvider>(*services);
```

For completion, start from `lsp::DefaultCompletionProvider` and override its
protected hooks instead of replacing the whole feature pipeline. The API is
documented in the [completion provider reference](../reference/completion-provider.md).

## When to keep the defaults

Defaults tend to be good enough when:

- symbol lookup is already handled by the standard reference pipeline
- document symbols follow the AST structure directly
- folding ranges are mostly based on CST blocks
- selection ranges and fuzzy matching do not need domain knowledge

## Good defaults to keep

- document symbol collection
- folding ranges
- fuzzy matching
- selection ranges

## Typical language-specific overrides

- completion
- rename
- formatter
- hover
- reference or definition logic

## Practical approach

Do not replace several providers at once unless the language genuinely needs
it. A safer order is:

1. formatter
2. hover or completion
3. definition/references/rename only if the default behavior is not enough
