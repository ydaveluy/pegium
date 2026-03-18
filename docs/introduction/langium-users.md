# Pegium for Langium Users

Pegium is inspired by Langium, but it is not a direct port of Langium to C++.
The mental model is intentionally similar in the areas that matter to language
authors.

## Read this page if

- you already know Langium and want to transfer that intuition to Pegium
- you want a concept mapping before reading Pegium-specific APIs
- you want the quickest path from "Langium mindset" to "Pegium workflow"

## Concept mapping

| Langium concept | Pegium concept |
| --- | --- |
| Grammar rules | `TerminalRule`, `DataTypeRule`, `ParserRule` |
| AST nodes | `pegium::AstNode` subclasses |
| CST helpers | `pegium::CstNodeView` and `pegium::CstUtils` |
| Validation registry | `pegium::validation::ValidationRegistry` |
| Scope provider | `pegium::references::ScopeProvider` |
| Formatter | `pegium::lsp::AbstractFormatter` |
| Default LSP providers | `pegium::services::Services::lsp` |

## Main differences

- Pegium grammars are written in C++ expressions instead of Langium grammar
  files.
- Pegium uses a PEG-based parser DSL as the foundation of the grammar layer.
- C++ types and member pointers are used directly for AST construction and
  formatter selections.
- Service composition happens through the Pegium services layer instead of
  dependency injection modules in TypeScript.

## How to translate your intuition

If you are used to Langium:

- think of `PegiumParser` subclasses as the place where grammar structure lives
- think of `Rule<T>` and `Terminal<T>` as the Pegium equivalent of named grammar
  rules and terminals
- think of `AstNode` structs as the direct equivalent of Langium AST types
- think of `reference<T>` plus linker/scope services as the reference pipeline
- think of `AbstractFormatter` and `ValidationRegistry` as close conceptual
  equivalents of the Langium formatter and validation registries

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

## Migration mindset

If you already know Langium, start with:

1. [Write the Grammar](../learn/workflow/write_grammar.md)
2. [Resolve Cross-References](../learn/workflow/resolve_cross_references.md)
3. [Create Validations](../learn/workflow/create_validations.md)
4. [Add Formatting and LSP Services](../learn/workflow/generate_everything.md)
5. [Default LSP services](../reference/lsp-services.md)

Then keep [Reference](../reference/index.md) nearby for the precise service and
document-model terminology.
