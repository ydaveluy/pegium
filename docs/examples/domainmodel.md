# DomainModel

Source: [examples/domainmodel](https://github.com/ydaveluy/pegium/tree/main/examples/domainmodel)

`domainmodel` is the clearest example of a declaration-oriented DSL in Pegium.
It is a good bridge between the compact `arithmetics` example and more
workspace-oriented examples.

## What it shows

- a modeling language for packages, entities, and datatypes
- structured AST nodes
- formatting with block-style layout
- rename and editor support through LSP

## Useful entry points

- CLI: `./build/examples/domainmodel/pegium-example-domainmodel-cli`
- LSP: `./build/examples/domainmodel/pegium-example-domainmodel-lsp`

## What to read first

The most useful reading path is usually:

- `examples/domainmodel/src/DomainModelModule.cpp` for service composition
- `examples/domainmodel/include/domainmodel/ast.hpp` for declarations built on
  `pegium::NamedAstNode`
- `examples/domainmodel/src/references/QualifiedNameProvider.cpp` for
  qualified-name construction
- `examples/domainmodel/src/references/DomainModelScopeComputation.cpp` for
  qualified exports and local symbols
- `examples/domainmodel/src/lsp/DomainModelRenameProvider.cpp` for rename
  behavior
- `examples/domainmodel/src/lsp/DomainModelFormatter.cpp` for block-oriented
  formatting

## Why start here

Use this example when your language looks like a modeling DSL rather than an
expression language.

## Use this example when

- the language has declarations, nesting, and blocks
- qualified names matter
- you want a clearer example of references and rename than `arithmetics`
- you want a formatter centered on keywords and brace blocks

## Continue with

- [Qualified Names](../recipes/scoping/qualified-names.md)
- [Custom Scope Provider](../recipes/custom-scope-provider.md)
- [LSP Services](../build-a-language/lsp-services.md)
