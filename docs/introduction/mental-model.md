# Pegium Mental Model

This page gives a quick concept mapping for readers who want a high-level
mental model before diving into Pegium-specific APIs.

## Concept mapping

| General concept | Pegium concept |
| --- | --- |
| Grammar rules | `Terminal<T>`, `Rule<T>`, `Infix<...>` |
| AST nodes | `pegium::AstNode` subclasses |
| CST helpers | `pegium::CstNodeView` and `pegium::CstUtils` |
| Validation registry | `pegium::validation::ValidationRegistry` |
| Scope provider | `pegium::references::ScopeProvider` |
| Formatter | `pegium::AbstractFormatter` |
| Default LSP providers | `pegium::Services::lsp` |

## Main differences

- Pegium grammars are written in C++ expressions instead of external grammar
  files.
- Pegium uses a PEG-based parser DSL as the foundation of the grammar layer.
- C++ types and member pointers are used directly for AST construction and
  formatter selections.
- Service composition happens through the Pegium services layer and explicit
  registration APIs.

## How to translate your intuition

If you are used to language tooling frameworks:

- think of `PegiumParser` subclasses as the place where grammar structure lives
- think of `Rule<T>` and `Terminal<T>` as Pegium's named grammar rules and
  terminals
- think of `AstNode` structs as the semantic node types of your language
- think of `reference<T>` plus linker/scope services as the reference pipeline
- think of `AbstractFormatter` and `ValidationRegistry` as close conceptual
  equivalents of formatter and validation registries

The biggest mental shift is that grammar and language services are ordinary C++
code. You get stronger type coupling with the AST, but less of the declarative
grammar-file feel.

## What stays pleasantly similar

Even with the different implementation language, the workflow remains familiar:

1. define the grammar
2. shape the AST
3. compute scopes and link references
4. validate semantics
5. add formatting and editor features

## Recommended reading path

If you want the fastest orientation path, start with:

1. [Write the Grammar](../learn/workflow/write_grammar.md)
2. [Resolve Cross-References](../learn/workflow/resolve_cross_references.md)
3. [Create Validations](../learn/workflow/create_validations.md)
4. [Add Formatting and LSP Services](../learn/workflow/generate_everything.md)
5. [LSP Services](../build-a-language/lsp-services.md)

Then keep [Reference](../reference/index.md) nearby for the precise service and
document-model terminology.
