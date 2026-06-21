# LSP Services

Pegium exposes editor features through `pegium::Services`. One language service container owns both the semantic layer and the editor-facing layer for a language.

This page walks through the subsystem. For task-oriented overrides see [Recipes — Custom LSP Features](../recipes/custom-lsp-features.md); for completion-specific extension points see the [Completion Provider reference](../reference/completion-provider.md).

## Default providers

`Services::lsp` exposes slots for many LSP features, but `makeDefaultServices(...)` installs only a subset.

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

Other slots are available but stay empty until your language installs an implementation: formatter, semantic tokens, signature help, selection ranges, call hierarchy, type hierarchy, code lens, and inlay hints.

Shared runtime services under `sharedServices.lsp` also get a default baseline:

- `textDocuments`
- `documentUpdateHandler`
- `fuzzyMatcher`
- `languageServer`
- `nodeKindProvider`
- `workspaceSymbolProvider`

For most languages, this baseline is enough to get an editor session running.

## Customization strategy

Start from `makeDefaultServices(...)`, then replace only the language-specific providers. This keeps the baseline behavior while you override completion, formatting, hover, or navigation logic where needed.

Typical setup:

```cpp
// Inside your install-module functions, on your own `MyServices` container:
auto services = pegium::makeDefaultServices<MyServices>(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
services->lsp.hoverProvider = std::make_unique<lsp::MyHoverProvider>(*services);
```

Formatter, hover, completion, and rename are the first features most languages customize. Many navigation features work from the default services as soon as references and scopes are correct.

## File operations

Install a custom `sharedServices->lsp.fileOperationHandler` only when your language server needs workspace file create/rename/delete hooks.

## Document update contract

`sharedServices->lsp.documentUpdateHandler` is the public hook for text document lifecycle events and watched-files updates.

Most languages keep the default handler. Override it only when save hooks or watched-file patterns are part of your language behavior.

## Practical advice

Do not replace several providers at once unless the language needs it. A safer order:

1. formatter
2. hover or completion
3. definition/references/rename only if the default behavior is not enough

For completion, extend `lsp::DefaultCompletionProvider` rather than rewriting it from scratch. The hooks are documented in the [completion provider reference](../reference/completion-provider.md).

## Related pages

- [Recipes — Custom LSP Features](../recipes/custom-lsp-features.md)
- [Completion Provider reference](../reference/completion-provider.md)
