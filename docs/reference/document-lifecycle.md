# Document Lifecycle

`workspace::Document` is Pegium's central workspace data structure: one source file as parsed text plus everything derived from it. Before editor features can rely on a document, `DocumentBuilder` builds it through a staged pipeline so features can depend on a minimum stable state instead of rebuilding everything ad hoc.

## Document states

A document moves through these states in order:

1. `Changed`
2. `Parsed`
3. `IndexedContent`
4. `ComputedScopes`
5. `Linked`
6. `IndexedReferences`
7. `Validated`

## Creation of documents

`DefaultDocumentFactory` turns source text into a managed document with AST, CST, diagnostics, and workspace metadata.

## Indexing of symbols

The first phase after parsing is `IndexedContent`. `ScopeComputation` and the index manager collect the symbols a document exports to the rest of the workspace, and those symbols become part of the global lookup space used later by scoping and linking.

This phase runs early on purpose: it depends only on the parsed tree, not on linked references.

## Computing scopes

After exported symbols are indexed, the document reaches `ComputedScopes`. This phase prepares the local symbol information that scope providers later use to answer "what is visible from this reference site?".

## Linking

With scopes available, the linker resolves references and moves the document to `Linked`. From here, features like go to definition, references, rename, and many semantic validations reason about concrete targets instead of unresolved names.

## Indexing of references

After linking, the document enters `IndexedReferences`. Reference descriptions are collected so Pegium knows which documents depend on which others, which is what makes incremental rebuilds possible.

## Validation

The final phase is `Validated`. `DocumentValidator` combines parse diagnostics, linking diagnostics, and custom validation checks into the diagnostics that editor clients and CLI tools care about.

Each feature waits for the minimum state it needs:

- formatting needs only parsed structure
- completion and hover want at least linked references
- rename and workspace navigation benefit from indexed references
- published diagnostics correspond to the validated state

## Workspace readiness versus document state

Workspace readiness and document phase are different concepts. `WorkspaceManager::ready()` only means startup documents have been discovered. When a feature needs `Linked` or `Validated`, wait for that phase explicitly.

## Modifications of a document

When source text changes, Pegium invalidates the derived analysis state, rebuilds the changed document, and uses indexed references to determine which other documents may need relinking or revalidation.

## Related pages

- [Workspace Lifecycle](../build-a-language/workspace.md)
- [Reference](index.md)
- [Configuration Services](configuration-services.md)
