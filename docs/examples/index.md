# Examples

Start from working code instead of the abstract API docs. Pegium ships four examples covering complementary aspects of the framework.

## Overview

| Example | Highlights |
| --- | --- |
| `arithmetics` | compact language, evaluator, formatter, LSP |
| `domainmodel` | DomainModel DSL, structure-first AST, formatter, rename |
| `requirements` | multi-language workspace, references, richer scoping |
| `statemachine` | modeling DSL, validation, formatter, LSP |

## Choose a starting point

- `arithmetics` — the smallest end-to-end example.
- `domainmodel` — a DomainModel-style DSL with entities, nesting, and formatting rules.
- `requirements` — multiple related languages or files.
- `statemachine` — a graph- or state-oriented domain.

## Practical advice

To reuse an example:

1. Choose the closest domain shape.
2. Keep the project structure and service wiring.
3. Shrink the grammar and AST to your actual needs.
4. Add back domain-specific validation, references, and formatting.

Read them as a progression in this order:

1. `arithmetics` — the smallest full parser-to-editor path.
2. `domainmodel` — nested declarations, qualified names, and rename.
3. `statemachine` — validation-heavy modeling.
4. `requirements` — multiple related languages in one workspace.

## Running them

After building the repository, each example exposes matching CLI and LSP entrypoints under `./build/examples/...`.

```bash
./build/examples/arithmetics/pegium-example-arithmetics-cli \
  eval examples/arithmetics/example/example.calc

./build/examples/arithmetics/pegium-example-arithmetics-lsp
```

The CLI takes a subcommand (`eval`) before the file path. Run it with no arguments to see the usage string.

## Related pages

- [Learn Pegium](../learn/index.md)
- [Recipes](../recipes/index.md)
- [Arithmetics](arithmetics.md)
