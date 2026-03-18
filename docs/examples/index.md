# Examples

Pegium ships four examples that cover complementary aspects of the framework.

Use this section when you want to start from working code rather than from the
abstract API documentation.

## Overview

| Example | Highlights |
| --- | --- |
| `arithmetics` | compact language, evaluator, formatter, LSP |
| `domainmodel` | DomainModel DSL, structure-first AST, formatter, rename |
| `requirements` | multi-language workspace, references, richer scoping |
| `statemachine` | modeling DSL, validation, formatter, LSP |

## Choose a starting point

- Use `arithmetics` if you want the smallest end-to-end example.
- Use `domainmodel` if you want a DomainModel-style DSL with entities,
  nesting, and formatting rules.
- Use `requirements` if you need multiple related languages or files.
- Use `statemachine` if your domain is graph- or state-oriented.

## How to use the examples

The best way to reuse an example is usually:

1. choose the closest domain shape
2. keep the project structure and service wiring
3. shrink the grammar and AST to your actual needs
4. add back domain-specific validation, references, and formatting

## Where to go next

- [Learn Pegium](../learn/index.md)
- [Recipes](../recipes/index.md)
- [Examples: Arithmetics](arithmetics.md)
