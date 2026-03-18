# Default LSP Services

Pegium exposes provider slots for a broad set of LSP features. A smaller
subset is installed automatically by `makeDefaultServices(...)`.

Use this page when you want to know which editor features come for free and
which ones still need explicit installation.

## `Services::lsp`

The `services.lsp` group contains providers such as:

- `completionProvider`
- `hoverProvider`
- `documentSymbolProvider`
- `documentHighlightProvider`
- `foldingRangeProvider`
- `declarationProvider`
- `definitionProvider`
- `typeProvider`
- `implementationProvider`
- `referencesProvider`
- `renameProvider`
- `codeActionProvider`
- `documentLinkProvider`
- `selectionRangeProvider`
- `signatureHelp`
- `codeLensProvider`
- `formatter`
- `inlayHintProvider`
- `semanticTokenProvider`
- `callHierarchyProvider`
- `typeHierarchyProvider`

Each slot is independent. You can replace one provider without rewriting the
entire language server setup.

Today, the default LSP module installs providers for completion, hover,
document symbols, document highlights, folding ranges, definition, references,
rename, and code actions. Other slots remain available but start as `nullptr`
until your language installs them.

## Default customization style

Use the defaults whenever your language follows common patterns, then override
the providers that are directly tied to language-specific semantics.

Typical installation:

```cpp
services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
services->lsp.renameProvider = std::make_unique<lsp::MyRenameProvider>(*services);
```

## Good defaults to keep

- document symbol collection
- document highlights
- folding ranges
- definition and references
- rename when the reference model is already correct

## Typical language-specific overrides

- completion
- rename
- formatter
- hover
- reference or definition logic

Completion has dedicated generic hooks in
[`lsp::DefaultCompletionProvider`](completion-provider.md), including
reference filtering, rule hooks, keyword hooks, and snippet generation.

## Good rule of thumb

Keep the defaults until a real product need appears. Replacing providers too
early usually increases maintenance cost without adding much value.

## Related pages

- [Configuration Services](configuration-services.md)
- [Completion Provider](completion-provider.md)
- [Custom LSP Features](../recipes/custom-lsp-features.md)
- [Formatting](../recipes/custom-formatter.md)
