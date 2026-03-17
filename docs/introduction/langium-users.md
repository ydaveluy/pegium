# Pegium for Langium Users

Pegium is inspired by Langium, but it is not a direct port of Langium to C++.
The mental model is intentionally similar in the areas that matter to language
authors.

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

- think of `Parser` subclasses as the place where grammar structure lives
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

1. [Grammar essentials](../build-a-language/grammar.md)
2. [References and scoping](../build-a-language/references-and-scoping.md)
3. [Validation](../build-a-language/validation.md)
4. [Formatting](../build-a-language/formatting.md)
5. [Default LSP services](../reference/lsp-services.md)
