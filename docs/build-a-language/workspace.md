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

## Practical guidance

- keep document parsing deterministic
- avoid custom workspace behavior until your language actually needs it
- customize scoping or indexing first; customize the workspace infrastructure
  itself only when the default lifecycle is too restrictive
