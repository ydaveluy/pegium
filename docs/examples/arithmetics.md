# Arithmetics

Source: [examples/arithmetics](https://github.com/ydaveluy/pegium/tree/main/examples/arithmetics)

`arithmetics` is the smallest example that still shows the full Pegium loop: parser, language services, validation, formatting, CLI, LSP server, and VS Code client.

## What it shows

- a parser for `.calc`
- a small evaluator
- a formatter
- a stdio LSP server
- a VS Code client

## Entry points

- CLI: `./build/examples/arithmetics/pegium-example-arithmetics-cli`
- LSP: `./build/examples/arithmetics/pegium-example-arithmetics-lsp`

## What to read first

- `examples/arithmetics/src/arithmetics/core/Parser.hpp` for the grammar and rule definitions
- `examples/arithmetics/src/arithmetics/core/ast.hpp` for the AST shape
- `examples/arithmetics/src/arithmetics/core/Module.cpp` for service wiring
- `examples/arithmetics/src/arithmetics/core/Language.cpp` for evaluating the parsed AST
- `examples/arithmetics/src/arithmetics/core/validation/ArithmeticsValidator.cpp` for semantic checks
- `examples/arithmetics/src/arithmetics/lsp/ArithmeticsFormatter.cpp` for formatter rules

## Use this example when

- your language is expression-heavy
- you need precedence and associativity
- you want to study `InfixRule` (precedence-climbing infix expressions)
- you want the smallest full formatter example

## Related pages

- [Grammar Essentials](../build-a-language/grammar.md)
- [Dependency Loops](../recipes/validation/dependency-loops.md)
- [Formatting](../build-a-language/formatting.md)
