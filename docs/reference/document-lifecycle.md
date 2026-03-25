# Document Lifecycle

`workspace::Document` is the central data structure of Pegium's workspace
layer. It represents one source file as parsed text plus all the information
derived from it.

Before a document can be used reliably by editor features, it has to be built
through a staged pipeline managed by `DocumentBuilder`.

## Document states

A Pegium document moves through these states:

1. `Changed`
2. `Parsed`
3. `IndexedContent`
4. `ComputedScopes`
5. `Linked`
6. `IndexedReferences`
7. `Validated`

The important idea is that later features can depend on a minimum stable state
instead of rebuilding everything ad hoc.

## Creation of documents

`DefaultDocumentFactory` is responsible for turning source text into a managed
document with AST, CST, diagnostics, and workspace metadata.

## Indexing of symbols

The first build phase after parsing is `IndexedContent`.

In this phase, `ScopeComputation` and the index manager collect the symbols
that a document exports to the rest of the workspace. Those exported symbols
become part of the global lookup space used later by scoping and linking.

This phase is intentionally early: it only depends on the parsed tree, not on
linked references.

## Computing scopes

After exported symbols are indexed, the document moves to `ComputedScopes`.

This phase prepares the local symbol information that later scope providers use
to answer the question "what is visible from this reference site?".

## Linking

Once scopes are available, the linker can resolve references and move the
document to `Linked`.

At the end of this phase, editor features such as go to definition, references,
rename, and many semantic validations can reason about concrete targets instead
of unresolved names.

## Indexing of references

After linking, the document enters `IndexedReferences`.

In this phase, reference descriptions are collected so Pegium knows which
documents depend on which others. This is what makes incremental rebuilds
possible.

## Validation

The final build phase is `Validated`.

`DocumentValidator` combines parse diagnostics, linking diagnostics, and custom
validation checks into the diagnostics that editor clients and CLI tools
usually care about.

## Why this matters

This lifecycle is what makes the rest of the framework coherent.

Typical examples:

- formatting only needs parsed structure
- completion and hover usually want at least linked references
- rename and workspace navigation benefit from indexed references
- published diagnostics normally correspond to the validated state

## Workspace readiness versus document state

Workspace readiness and document phase are intentionally different concepts.
`WorkspaceManager::ready()` only means startup documents have been discovered.
When a feature needs `Linked` or `Validated`, it should wait for that phase
explicitly.

## Modifications of a document

When the source text changes, Pegium invalidates the derived analysis state,
rebuilds the changed document, and uses indexed references to determine which
other documents may need relinking or revalidation.

## Related pages

- [Workspace Lifecycle](../build-a-language/workspace.md)
- [Reference](index.md)
- [Configuration Services](configuration-services.md)
