# Arithmetics

Source: [examples/arithmetics](https://github.com/ydaveluy/pegium/tree/main/examples/arithmetics)

`arithmetics` is the smallest example that still shows the full Pegium loop:
parser, language services, validation, formatting, CLI, LSP server, and VS
Code client.

## What it shows

- a parser for `.calc`
- a small evaluator
- a formatter
- a stdio LSP server
- a VS Code client

## Useful entry points

- CLI: `./build/examples/arithmetics/pegium-example-arithmetics-cli`
- LSP: `./build/examples/arithmetics/pegium-example-arithmetics-lsp`

## What to read first

If you want to understand the example quickly, start with:

- `examples/arithmetics/src/ArithmeticsModule.cpp` for service wiring
- `examples/arithmetics/src/Language.cpp` for the grammar and AST shaping
- `examples/arithmetics/src/validation/ArithmeticsValidator.cpp` for semantic
  checks
- `examples/arithmetics/src/lsp/ArithmeticsFormatter.cpp` for formatter rules

## Why start here

This is the smallest example that still exercises parsing, services, validation,
formatting, and the editor integration path.

## Use this example when

- your language is expression-heavy
- you need precedence and associativity
- you want to study `Infix` rules
- you want the smallest full formatter example

## Continue with

- [Write the Grammar](../learn/workflow/write_grammar.md)
- [Dependency Loops](../recipes/validation/dependency-loops.md)
- [Formatting](../build-a-language/formatting.md)
