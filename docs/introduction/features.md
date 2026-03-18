# Features

Pegium combines the core pieces of a textual language workbench in one C++20
framework.

## Read this page if

- you want a quick inventory of what Pegium provides
- you need a concise feature overview for a teammate or stakeholder
- you want to jump from capabilities to the right doc section

## Grammar and parsing

- grammars are written directly in C++ through `PegiumParser`
- parser expressions cover terminals, named rules, repetitions, lookahead, and
  infix parsing
- recovery-aware parsing and expectation tracing are built into the parser
  runtime

## Semantic model

- AST nodes are regular C++ types
- CST nodes remain available for source-aware features
- references keep both the written text and the resolved target

## Language services

- references, scoping, linking, and validation are explicit services
- formatters are installed through typed formatter classes
- shared services support multi-document and multi-language workspaces

## Editor support

- language servers are built on top of the same document model
- default providers cover completion, hover, document symbols, references,
  rename, definition, and more
- the examples show complete CLI plus LSP setups

## Practical workflow

Pegium works best when you follow the same high-level loop throughout the
project:

1. write the grammar
2. shape the AST and CST
3. resolve references
4. create validations
5. add formatting and LSP behavior

## Continue with

- [Learn Pegium](../learn/index.md) for the recommended workflow
- [Showcases](showcases.md) for complete example languages
- [Choose Your Path](choose-your-path.md) if you are still deciding where to go
  next
