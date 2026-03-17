# Domainmodel

Source:
[examples/domainmodel](https://github.com/ydaveluy/pegium/tree/main/examples/domainmodel)

## What it shows

- a modeling language for packages, entities, and datatypes
- structured AST nodes
- formatting with block-style layout
- rename and editor support through LSP

## Useful entry points

- CLI: `./build/examples/domainmodel/pegium-example-domainmodel-cli`
- LSP: `./build/examples/domainmodel/pegium-example-domainmodel-lsp`

## Why start here

Use this example when your language looks like a modeling DSL rather than an
expression language.

## Use this example when

- the language has declarations, nesting, and blocks
- qualified names matter
- you want a clearer example of references and rename than `arithmetics`
- you want a formatter centered on keywords and brace blocks
