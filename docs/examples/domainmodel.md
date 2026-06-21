# DomainModel

Source: [examples/domainmodel](https://github.com/ydaveluy/pegium/tree/main/examples/domainmodel)

`domainmodel` is the clearest example of a declaration-oriented DSL in Pegium, bridging the compact `arithmetics` example and the more workspace-oriented ones.

## What it shows

- a modeling language for packages, entities, and datatypes
- structured AST nodes
- formatting with block-style layout
- rename and editor support through LSP

## Entry points

- CLI: `./build/examples/domainmodel/pegium-example-domainmodel-cli`
- LSP: `./build/examples/domainmodel/pegium-example-domainmodel-lsp`

## What to read first

- `examples/domainmodel/src/domainmodel/core/Module.cpp` for service composition
- `examples/domainmodel/src/domainmodel/core/ast.hpp` for declarations built on
  `pegium::NamedAstNode`
- `examples/domainmodel/src/domainmodel/core/references/QualifiedNameProvider.cpp` for
  qualified-name construction
- `examples/domainmodel/src/domainmodel/core/references/DomainModelScopeComputation.cpp` for
  qualified exports and local symbols
- `examples/domainmodel/src/domainmodel/lsp/DomainModelRenameProvider.cpp` for rename
  behavior
- `examples/domainmodel/src/domainmodel/lsp/DomainModelFormatter.cpp` for block-oriented
  formatting

## Use this example when

- your language is a modeling DSL rather than an expression language
- the language has declarations, nesting, and blocks
- qualified names matter
- you want a clearer example of references and rename than `arithmetics`
- you want a formatter centered on keywords and brace blocks

## Related pages

- [Qualified Names](../recipes/scoping/qualified-names.md)
- [Custom Scope Provider](../recipes/custom-scope-provider.md)
- [LSP Services](../build-a-language/lsp-services.md)
