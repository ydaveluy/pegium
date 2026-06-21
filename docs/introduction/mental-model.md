# Pegium Mental Model

A quick high-level model of how Pegium fits together, so the subsystem pages make sense.

## The five ideas to keep in mind

1. The grammar is ordinary C++ code.
2. The AST is your semantic model.
3. References are resolved after parsing, not during parsing.
4. Services assemble the language behavior.
5. The same document model powers parsing, diagnostics, and editor features.

## Main differences

- You write grammars as C++ expressions, not external grammar files.
- A PEG-based parser DSL is the foundation of the grammar layer.
- C++ types and member pointers drive AST construction and formatter selections.
- Service composition happens through explicit service wiring.

## How to translate your intuition

If you come from other language tooling frameworks:

- The parser class is the home of the grammar.
- Your AST structs are the language model you validate and traverse.
- Scoping and linking turn names into targets.
- The service container gives the language its parser, validator, formatter, and editor behavior.

The biggest shift: grammar and language services are ordinary C++ code. You get stronger type coupling with the AST, but less of the declarative grammar-file feel.

## What stays similar

The workflow is familiar:

1. Define the grammar.
2. Shape the AST.
3. Compute scopes and link references.
4. Validate semantics.
5. Add formatting and editor features.

## Related pages

- [Build a Language End-to-End](../learn/walkthrough.md)
- [Grammar Essentials](../build-a-language/grammar.md)
- [References and Scoping](../build-a-language/references-and-scoping.md)
- [Validation](../build-a-language/validation.md)
- [LSP Services](../build-a-language/lsp-services.md)
- [Reference](../reference/index.md) — precise service and document-model terminology.
