# LSP Services

Pegium exposes editor features through `pegium::Services`.

Core service containers live in `pegium::services`; the LSP aggregate
containers live directly in `pegium`.

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

Other slots such as formatter, selection ranges, signature help, semantic
tokens, call hierarchy, type hierarchy, inlay hints, and code lens are
available on `services->lsp`, but they stay empty until your language installs
an implementation.

Shared runtime services under `sharedServices.lsp` also get a default baseline:

- `textDocuments`
- `documentUpdateHandler`
- `fuzzyMatcher`
- `languageServer`
- `nodeKindProvider`
- `workspaceSymbolProvider`

The default `languageServer` already owns the standard initialization flow for
configuration and workspace services. The runtime layer just forwards protocol
messages and keeps transport concerns out of language modules.

During an active session, `sharedServices.lsp.languageClient` gives shared
services a narrow way to talk back to the editor for:

- capability registration
- configuration fetches

The raw transport object stays internal to the runtime layer.

In practice, one `Services` object gives you both the semantic layer and the
editor layer for a language.

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

When you override completion options, only advertise fields Pegium supports:

- `triggerCharacters`
- `allCommitCharacters`

For many languages, the defaults are already good enough for document symbols,
document highlights, folding ranges, and most navigation features. Formatter,
hover, completion, and rename are the first features that typically need
language-specific behavior.

## File operations

Install a custom `sharedServices->lsp.fileOperationHandler` only when your
language server needs workspace file create/rename/delete hooks.

Registration comes exclusively from
`FileOperationHandler::fileOperationOptions()`. If an operation is absent from
that object, Pegium does not announce it and does not wire its callback.

## Document update contract

`sharedServices->lsp.documentUpdateHandler` is the public hook for text
document lifecycle events and watched-files updates.

Capability advertisement is derived only from these methods:

- `supportsDidSaveDocument()`
- `supportsWillSaveDocument()`
- `supportsWillSaveDocumentWaitUntil()`

If you return `true`, implement the matching callback. If you leave the support
method at its default `false`, Pegium does not announce the capability and does
not wire the callback.

The default handler also performs watched-files dynamic registration when the
client supports it, then forwards `workspace/didChangeWatchedFiles` into the
workspace rebuild pipeline.

If you need custom watcher patterns, derive from
`DefaultDocumentUpdateHandler` and override the protected `getWatchers()`
hook. The default implementation registers a single workspace-wide `**/*`
watcher, and returning an empty list skips registration entirely.

Lifecycle work triggered during `initialized(...)` stays asynchronous and
non-blocking. Failures are reported through the shared observability sink
instead of being silently ignored.

## Practical approach

Do not replace several providers at once unless the language genuinely needs
it. A safer order is:

1. formatter
2. hover or completion
3. definition/references/rename only if the default behavior is not enough

Keep runtime internals out of language modules. The supported extension points
are the service slots and shared handlers, not the internal request dispatch
helpers used by the language server runtime.

For the executable itself, prefer `runLanguageServerMain(...)` over open-coded
connection/bootstrap logic in every example `main`.
