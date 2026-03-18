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

`Services::lsp` exposes slots for many LSP features, but only a subset is
installed automatically by `makeDefaultServices(...)`.

The current built-in defaults cover:

- completion
- hover
- document symbols
- document highlights
- folding ranges
- definition
- references
- rename
- code actions

Other slots such as formatter, selection ranges, signature help, semantic
tokens, call hierarchy, type hierarchy, inlay hints, and code lens are
available on `services->lsp`, but they stay empty until your language installs
an implementation.

## Customization strategy

Start from `makeDefaultServices(...)`, then replace only the providers that are
language-specific. This keeps the baseline behavior while letting you override
completion, formatting, hover, or navigation logic where needed.

Typical setup:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
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
- hover can come from leading comments or documentation providers

## Good defaults to keep

- document symbol collection
- document highlights
- folding ranges
- definition and references
- rename when references are modeled correctly
- the shared fuzzy matcher used by completion

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
