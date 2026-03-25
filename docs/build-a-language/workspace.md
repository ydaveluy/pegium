# Workspace Lifecycle

Pegium's workspace layer keeps documents, indexes, and text content in sync.

## Key pieces

- `DocumentFactory`
- `Documents`
- `TextDocuments`
- `DocumentBuilder`
- `IndexManager`
- `WorkspaceManager`
- `WorkspaceLock`

These services matter as soon as the language stops being “parse one file and
stop”.

## Lifecycle overview

1. text documents are opened or updated
2. Pegium rebuilds parse results and syntax trees
3. index data and exported symbols are refreshed
4. references, validation, and LSP features observe the updated document state

Internally, documents move through states such as:

- `Changed`
- `Parsed`
- `IndexedContent`
- `ComputedScopes`
- `Linked`
- `IndexedReferences`
- `Validated`

That staged lifecycle is what allows language features to depend on stable
intermediate results instead of reparsing or relinking ad hoc.

## `Document` versus `TextDocument`

Pegium deliberately separates source text from derived analysis.

- `Document` owns the parse result, AST/CST, references, diagnostics, and
  analysis state
- `TextDocument` owns the current source text, language id, version, and
  offset/position helpers

When you need source text or position conversions, use `document.textDocument()`.
When you need parse results, scopes, or diagnostics, use `Document`.

## Why this matters

Once a language grows beyond a single file, editor behavior depends on stable
workspace services. Scope resolution, rename, references, and workspace symbols
all rely on the same document and index infrastructure.

## What `DocumentBuilder` does

`DocumentBuilder` is the orchestrator of the pipeline. It is the reason a file
change turns into a coherent rebuild instead of a pile of ad hoc reparsing.

## Readiness versus stable phases

`WorkspaceManager::ready()` only means startup documents have been discovered.
If a feature needs a stronger guarantee such as `Linked` or `Validated`, wait
for that phase explicitly.

## Practical guidance

- keep parsing deterministic
- customize scoping and indexing before touching workspace internals
- treat the workspace as the shared foundation for references, diagnostics, and
  editor features
