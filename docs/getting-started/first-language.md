# Your First Language

The fastest path is to copy one of the examples and reduce it to the features
you need.

This page is a legacy shortcut. The current guided path lives in
[Learn Pegium](../learn/index.md).

Starting from an existing example is usually better than writing a fresh
language skeleton, because the examples already wire:

- the parser
- language services
- optional validator and formatter classes
- CLI and LSP binaries
- test coverage

## Recommended path

1. Start from `examples/arithmetics`.
2. Rename the AST nodes and grammar rules to match your language.
3. Keep the service wiring and the LSP executable shape intact.
4. Add validation, scoping, and formatting once parsing works.

## Minimal checklist

- define your AST node types
- implement the parser rules
- create language services with `makeDefaultServices(...)`
- register custom services such as a validator or formatter
- expose a CLI and, if needed, an LSP binary

## A practical migration strategy

When turning an example into your own language:

1. keep the overall project layout
2. rename the AST and parser types
3. reduce the grammar to the minimal constructs you need
4. keep the service wiring intact until the grammar stabilizes
5. add scoping, validation, and formatting in that order

This tends to produce fewer broken intermediate states than trying to design
every feature at once.

## Next steps

- [Write the Grammar](../learn/workflow/write_grammar.md)
- [Shape the AST and CST](../learn/workflow/generate_ast.md)
- [Add Formatting and LSP Services](../learn/workflow/generate_everything.md)
- [Custom validator recipe](../recipes/custom-validator.md)
