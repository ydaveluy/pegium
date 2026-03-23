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

When you need source text or text-coordinate conversions, use
`document.textDocument()`. The managed workspace keeps the canonical URI on the
`Document` stable while refreshing the authoritative `TextDocument` snapshot as
files change.
`TextDocument::getText()` exposes that current text as a borrowed
`std::string_view`, so consumers that need ownership should copy explicitly.

## Why this matters

Once a language grows beyond a single file, editor behavior depends on stable
workspace services. Scope resolution, rename, references, and workspace symbols
all rely on the same document and index infrastructure.

## What `DocumentBuilder` does

`DocumentBuilder` is the orchestrator of the pipeline. It rebuilds documents
through the successive analysis states and emits phase events that other
services can observe.

In practice, this is the part that makes “document changed, now update parse,
index, linking, diagnostics, and editor state” coherent.

The builder operates on managed documents only: non-null documents with a
normalized non-empty URI.

## Readiness versus stable phases

`WorkspaceManager::ready()` only means startup documents have been discovered
and materialized.

If a feature needs a real analysis milestone such as `Linked` or `Validated`,
wait explicitly with `DocumentBuilder::waitUntil(...)` instead of treating
workspace readiness as a stronger guarantee.

## Practical guidance

- keep document parsing deterministic
- avoid custom workspace behavior until your language actually needs it
- customize scoping or indexing first; customize the workspace infrastructure
  itself only when the default lifecycle is too restrictive
