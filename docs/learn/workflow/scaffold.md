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

If you are not sure, copy `arithmetics`. It exercises every layer (parser,
AST, validation, formatting, CLI, LSP) in the smallest amount of code.

## Concrete steps to adapt an example

The example layout uses a per-language namespace folder, so renaming is
mechanical. Suppose your language is named `mylang`:

1. Copy the example folder, for instance
   `examples/arithmetics` → `examples/mylang`.
2. Rename the inner namespace folder
   `examples/mylang/src/arithmetics/` → `examples/mylang/src/mylang/`.
3. In `examples/mylang/CMakeLists.txt`, replace every occurrence of
   `arithmetics` with `mylang` (target names, install paths,
   `add_subdirectory(...)`, include directories).
4. In every C++ source under the new tree, replace:
   - the namespace `arithmetics::...` with `mylang::...`
   - includes `<arithmetics/...>` with `<mylang/...>`
   - the parser class name `ArithmeticsParser` with `MylangParser`
   - the file extension `.calc` (in `Module.cpp`) with your own
5. Add the new example folder to the top-level `examples/CMakeLists.txt`.
6. Rebuild: `cmake --build build -j`.

At this point you should have a binary like
`build/examples/mylang/pegium-example-mylang-cli` that still parses the
arithmetics grammar but lives entirely under your own names. From there you
edit the grammar and AST types to make the language your own.

## What to keep for the first iteration

While you adapt the example, leave these pieces alone until the language is
parsing again:

- the overall project layout
- the service bootstrap (`Module.cpp`, `Services.hpp`)
- the CLI and LSP entrypoints (`cli_main.cpp`, `lsp_main.cpp`)
- the test structure

Once your renamed copy still builds and runs, you can start replacing the
grammar, AST, and services step by step.

## Recommended next step

Once you have a renamed copy that still builds, continue with
[Write the Grammar](write_grammar.md).
