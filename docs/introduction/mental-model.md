# Pegium Mental Model

This page gives a quick high-level model of how Pegium fits together before you
start reading the subsystem pages.

## The five ideas to keep in mind

1. The grammar is ordinary C++ code.
2. The AST is your semantic model.
3. References are resolved after parsing, not during parsing.
4. Services assemble the language behavior.
5. The same document model powers parsing, diagnostics, and editor features.

## Main differences

- Pegium grammars are written in C++ expressions instead of external grammar
  files.
- Pegium uses a PEG-based parser DSL as the foundation of the grammar layer.
- C++ types and member pointers are used directly for AST construction and
  formatter selections.
- Service composition happens through explicit service wiring.

## How to translate your intuition

If you are used to language tooling frameworks:

- think of the parser class as the home of the grammar
- think of your AST structs as the language model you will validate and
  traverse
- think of scoping and linking as the stage that turns names into targets
- think of the service container as the place where the language gets its
  parser, validator, formatter, and editor behavior

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
