# Showcases

Pegium ships several end-to-end examples that act as both showcases and
starting points for real projects.

## Read this page if

- you want to inspect complete Pegium languages before reading APIs
- you are choosing which example to clone or adapt
- you want to see which advanced feature each example emphasizes

## Arithmetics

The [arithmetics example](../examples/arithmetics.md) is the smallest complete
showcase. It includes:

- a grammar with precedence-aware expressions
- an evaluator
- validation and formatting
- a language server and VS Code client

## DomainModel

The [domainmodel example](../examples/domainmodel.md) shows a modeling DSL
with:

- nested declarations
- qualified names
- rename support
- formatter rules for block-oriented syntax

## Requirements

The [requirements example](../examples/requirements.md) demonstrates:

- multiple related languages
- shared workspace behavior
- cross-file and cross-language references
- one LSP server serving more than one language id

## Statemachine

The [statemachine example](../examples/statemachine.md) focuses on:

- semantic validation
- state-oriented modeling
- formatting and editor integration

## Recommended first showcase

If you only want to inspect one showcase before learning Pegium, start with
[arithmetics](../examples/arithmetics.md). It is the most compact example that
still exercises the full parser-to-editor path.

## Continue with

- [Examples Overview](../examples/index.md)
- [Learn Pegium](../learn/index.md)
- [Recipes](../recipes/index.md) once you know which subsystem you want to
  customize
