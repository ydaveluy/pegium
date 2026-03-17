# Examples

Pegium ships four examples that cover complementary aspects of the framework.

## Overview

| Example | Highlights |
| --- | --- |
| `arithmetics` | compact language, evaluator, formatter, LSP |
| `domainmodel` | modeling language, structure-first AST, formatter, rename |
| `requirements` | multi-language workspace, references, richer scoping |
| `statemachine` | modeling DSL, validation, formatter, LSP |

## Choose a starting point

- Use `arithmetics` if you want the smallest end-to-end example.
- Use `domainmodel` if you want entities, nesting, and formatting rules.
- Use `requirements` if you need multiple related languages or files.
- Use `statemachine` if your domain is graph- or state-oriented.

## How to use the examples

The best way to reuse an example is usually:

1. choose the closest domain shape
2. keep the project structure and service wiring
3. shrink the grammar and AST to your actual needs
4. add back domain-specific validation, references, and formatting
