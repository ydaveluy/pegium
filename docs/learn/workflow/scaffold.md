# 2. Choose a Starting Point

Pegium does not currently revolve around a separate scaffolding generator. The
recommended equivalent is to start from one of the shipped examples and reduce
it to your actual needs.

## Why start from an example

The examples already solve the boring but important wiring:

- parser ownership
- services registration
- CLI entry points
- LSP executable shape
- tests and formatting hooks

This makes them the practical Pegium equivalent of a scaffolded project.

## Which example to choose

- use [arithmetics](../../examples/arithmetics.md) for the smallest complete
  parser-to-editor example
- use [domainmodel](../../examples/domainmodel.md) for nested declarations and
  rename
- use [requirements](../../examples/requirements.md) for multi-language and
  workspace behavior
- use [statemachine](../../examples/statemachine.md) for validation-heavy
  modeling DSLs

## Recommended next step

Once you have chosen a starting point, continue with
[Write the Grammar](write_grammar.md).
