# LSP Services

Pegium exposes editor features through `pegium::Services`.

The main idea is simple: one language service container owns both the semantic
layer and the editor-facing layer for a language.

## Default providers

`Services::lsp` exposes slots for many LSP features, but only a subset is
installed automatically by `makeDefaultServices(...)`.

The built-in defaults cover:

- completion
- hover
- document symbols
- document highlights
- folding ranges
- definition
- references
- rename
- code actions

Other slots such as formatter, semantic tokens, signature help, selection
ranges, call hierarchy, type hierarchy, code lens, and inlay hints are
available too, but they stay empty until your language installs an
implementation.

Shared runtime services under `sharedServices.lsp` also get a default baseline:

- `textDocuments`
- `documentUpdateHandler`
- `fuzzyMatcher`
- `languageServer`
- `nodeKindProvider`
- `workspaceSymbolProvider`

For most languages, that default baseline is already enough to get an editor
session running.

## Customization strategy

Start from `makeDefaultServices(...)`, then replace only the providers that are
language-specific. This keeps the baseline behavior while letting you override
completion, formatting, hover, or navigation logic where needed.

Typical setup:

```cpp
auto services = pegium::makeDefaultServices(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
services->lsp.hoverProvider = std::make_unique<lsp::MyHoverProvider>(*services);
```

In practice, formatter, hover, completion, and rename are the first features
that most languages customize. Many navigation features work well from the
default services as soon as references and scopes are correct.

## File operations

Install a custom `sharedServices->lsp.fileOperationHandler` only when your
language server needs workspace file create/rename/delete hooks.

## Document update contract

`sharedServices->lsp.documentUpdateHandler` is the public hook for text
document lifecycle events and watched-files updates.

Most languages can keep the default handler. Override it only when save hooks
or watched-file patterns are really part of your language behavior.

## Practical approach

Do not replace several providers at once unless the language genuinely needs
it. A safer order is:

1. formatter
2. hover or completion
3. definition/references/rename only if the default behavior is not enough

For completion, prefer extending `lsp::DefaultCompletionProvider` rather than
rewriting completion from scratch. The relevant hooks are documented in the
[completion provider reference](../reference/completion-provider.md).
