# Default LSP Services

Pegium ships default implementations for a broad set of LSP features.

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
- folding ranges
- fuzzy matching
- selection ranges

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
