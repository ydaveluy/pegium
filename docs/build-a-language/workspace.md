# Workspace Lifecycle

The workspace layer keeps documents, indexes, and text content in sync. You need it as soon as your language grows beyond "parse one file and stop".

## Key pieces

- `DocumentFactory`
- `Documents`
- `TextDocuments`
- `DocumentBuilder`
- `IndexManager`
- `WorkspaceManager`
- `WorkspaceLock`

## Lifecycle overview

1. Text documents are opened or updated.
2. Pegium rebuilds parse results and syntax trees.
3. Index data and exported symbols are refreshed.
4. References, validation, and LSP features observe the updated document state.

Documents move through staged states:

- `Changed`
- `Parsed`
- `IndexedContent`
- `ComputedScopes`
- `Linked`
- `IndexedReferences`
- `Validated`

This staging lets language features depend on stable intermediate results instead of reparsing or relinking ad hoc.

## `Document` versus `TextDocument`

Pegium separates source text from derived analysis.

- `Document` owns the parse result, AST/CST, references, diagnostics, and analysis state.
- `TextDocument` owns the current source text, language id, version, and offset/position helpers.

Use `document.textDocument()` for source text or position conversions. Use `Document` for parse results, scopes, or diagnostics.

Scope resolution, rename, references, and workspace symbols all rely on this shared document and index infrastructure.

## What `DocumentBuilder` does

`DocumentBuilder` orchestrates the pipeline. It turns a file change into a coherent rebuild instead of a pile of ad hoc reparsing.

## Readiness versus stable phases

`WorkspaceManager::ready()` only means startup documents have been discovered. If a feature needs a stronger guarantee such as `Linked` or `Validated`, wait for that phase explicitly.

## Practical advice

- Keep parsing deterministic.
- Customize scoping and indexing before touching workspace internals.
- Treat the workspace as the shared foundation for references, diagnostics, and editor features.
