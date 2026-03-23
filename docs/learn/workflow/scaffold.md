# 2. Choose a Starting Point

Pegium does not ask you to start from an empty directory. The fastest way to
create a first working language is to start from one of the shipped examples
and adapt it to your needs.

## Why start from an example

The examples already solve the boring but important wiring:

- parser ownership
- services registration
- CLI entry points
- LSP executable shape
- tests and formatting hooks

This gives you a concrete project skeleton with real parser, services, and
tooling wiring already in place.

## Which example to choose

- use [arithmetics](../../examples/arithmetics.md) for the smallest complete
  parser-to-editor example
- use [domainmodel](../../examples/domainmodel.md) for nested declarations and
  rename
- use [requirements](../../examples/requirements.md) for multi-language and
  workspace behavior
- use [statemachine](../../examples/statemachine.md) for validation-heavy
  modeling DSLs

## What to keep first

When you copy an example, keep the following pieces intact for the first
iteration:

- the overall project layout
- the service bootstrap
- the CLI and LSP entrypoints
- the test structure

Then rename the parser, AST types, namespaces, and file associations to match
your language.

## Recommended next step

Once you have chosen a starting point, continue with
[Write the Grammar](write_grammar.md).
