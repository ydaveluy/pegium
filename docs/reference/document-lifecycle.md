# Document Lifecycle

`workspace::Document` is the central data structure of Pegium's workspace
layer. It represents one source file as parsed text plus all the information
derived from it.

Before a document can be used reliably by editor features, it has to be built
through a staged pipeline managed by `DocumentBuilder`.

Use this page when you need to know when parsing, indexing, linking,
validation, or cache invalidation happen.

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

## How a document is created

`DefaultDocumentFactory` is responsible for turning text into a
`workspace::Document`.

In practice, that means:

- taking text from `TextDocuments` or the file system
- resolving the language id
- selecting the registered parser
- parsing into AST and CST
- putting the document into the `Parsed` state

## What happens after parsing

### IndexedContent

Exported symbols are described and stored so that other documents can later see
them.

### ComputedScopes

Local symbols are prepared so that references can ask “what is visible from
here?”.

### Linked

References are resolved against the current scope and target descriptions.

### IndexedReferences

Reference metadata is indexed so that cross-document dependencies are known.

### Validated

Parse diagnostics, linking issues, and custom validation checks are translated
into the final document diagnostics.

## Why this matters

This lifecycle is what makes the rest of the framework coherent.

Typical examples:

- formatting only needs parsed structure
- completion or hover usually want at least linked references
- rename and workspace navigation benefit from indexed references
- published diagnostics normally correspond to the validated state

## What changes do

When the source text changes, the document is invalidated and moved back toward
`Changed`. The builder then reruns the necessary later phases. This is what
lets Pegium keep multiple documents and editor requests in sync without
throwing away the whole workspace every time one file changes.

## Related pages

- [Workspace Concepts](workspace.md)
- [Start Here](start-here.md)
- [Configuration Services](configuration-services.md)
