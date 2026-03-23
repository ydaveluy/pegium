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

`DefaultDocumentFactory` is responsible for turning text into a
`workspace::Document`.

When the workspace manager or the LSP runtime needs a document, the factory:

- gets the latest text from the shared text-document provider or the file
  system
- materializes a `Document` from the authoritative `TextDocument`
- fixes the canonical document URI at construction time
- resolves the language services from the document URI
- normalizes the attached text-document `languageId` from those services
- selects the registered parser
- parses the text into AST and CST
- stores the document in the `Parsed` state

If the URI does not resolve to registered services, document creation fails
instead of producing a partially typed workspace document.

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
to answer the question "what is visible from this reference site?". In other
words, it turns the parsed tree into a document that is ready for linking.

This is also why scope computation should not depend on already linked
references. Linked references happen later in the lifecycle.

## Linking

Once scopes are available, the linker can resolve references and move the
document to `Linked`.

Conceptually, linking combines:

1. the written reference text
2. the visible candidates exposed by the scope provider
3. the target descriptions and target documents managed by the workspace

At the end of this phase, editor features such as go to definition, references,
rename, and many semantic validations can reason about concrete targets instead
of just unresolved names.

## Indexing of references

After linking, the document enters `IndexedReferences`.

In this phase, reference descriptions are collected and stored in the workspace
index so that Pegium knows which documents depend on which others. This
dependency information is what makes incremental rebuilds possible: when one
document changes, Pegium can identify which other documents need to be
relinked or revalidated.

## Validation

The final build phase is `Validated`.

`DocumentValidator` combines:

- parse diagnostics
- linking diagnostics
- custom validation checks from the validation registry

The resulting diagnostics are the stable diagnostics that editor clients and
CLI tools usually care about.

## Why this matters

This lifecycle is what makes the rest of the framework coherent.

Typical examples:

- formatting only needs parsed structure
- completion or hover usually want at least linked references
- rename and workspace navigation benefit from indexed references
- published diagnostics normally correspond to the validated state

## Workspace readiness versus document state

Workspace readiness and document phase are intentionally different concepts.

`WorkspaceManager::ready()` only tells you that startup documents have been
loaded into the workspace. The initial build may still be running when that
happens.

If a newer change arrives during bootstrap, the workspace lock may supersede
the tail of that older build. This is a normal part of the lifecycle, not a
consistency failure. `DocumentBuilder` resumes from the last completed phase
and rebuilds toward the newest text snapshot.

When a consumer needs a stable phase such as `Linked` or `Validated`, it
should wait explicitly with `DocumentBuilder::waitUntil(...)`.

## Modifications of a document

`TextDocument` owns the source text, the current document version, and the
offset/position helpers.
`Document` keeps the analysis derived from that text plus the stable
canonical URI used by the managed workspace.

When the source text changes, managed updates refresh the current
`TextDocument` through the document factory, invalidate the derived analysis
state when needed, and move the document back toward `Changed`. The same
`Document` instance and the same canonical URI stay in place while the builder
reruns the necessary later phases instead of rebuilding the whole workspace
from scratch.

The backing `TextDocument` keeps an internal immutable text snapshot so the
parser and CST can retain a stable view of the analyzed text without copying
the whole source on every consumer access.
`TextDocument::getText()` therefore returns a borrowed `std::string_view` over
the current snapshot; take an explicit `std::string` copy only when mutation or
ownership is required.

Changing only the current text-document `languageId` does not trigger that
incremental invalidation path. To rebuild a document under a different
language, rematerialize it from a new text document.

This is where the indexed reference information becomes important. Pegium can
use it to determine which other documents may have been affected by the change
and reset only those documents to the earliest phase they need to rerun.

## Related pages

- [Workspace Lifecycle](../build-a-language/workspace.md)
- [Reference](index.md)
- [Configuration Services](configuration-services.md)
