# Showcases

Pegium ships several end-to-end examples. They are both showcases of the
framework and practical starting points for real projects.

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

## Where to go from here?

- Continue with the [examples overview](../examples/index.md) if you want to
  compare the examples side by side.
- Continue with the [workflow](../learn/workflow/index.md) if you want to
  build your own language next.
- Continue with [recipes](../recipes/index.md) once you know which subsystem
  you want to customize.
