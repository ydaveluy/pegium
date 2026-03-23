# Statemachine

Source: [examples/statemachine](https://github.com/ydaveluy/pegium/tree/main/examples/statemachine)

`statemachine` is a compact modeling example where semantic rules matter more
than expression parsing. It is especially useful when you want to study
whole-model validation and editor support together.

## What it shows

- a state-oriented modeling language
- semantic validation rules
- formatting and LSP integration
- a VS Code client for interactive testing

## Useful entry points

- CLI: `./build/examples/statemachine/pegium-example-statemachine-cli`
- LSP: `./build/examples/statemachine/pegium-example-statemachine-lsp`

## What to read first

Start with:

- `examples/statemachine/src/StatemachineModule.cpp` for the service setup
- `examples/statemachine/src/validation/StatemachineValidator.cpp` for typed
  validation checks
- `examples/statemachine/src/lsp/StatemachineFormatter.cpp` for the formatter

## Why start here

Use this example when your language centers on connected model elements and
validation rules rather than expression evaluation.

## Use this example when

- semantic validation is central to the language
- the domain is graph- or state-based
- you want a compact modeling example with editor support

## Continue with

- [Validation](../recipes/validation/index.md)
- [Custom Validator](../recipes/custom-validator.md)
- [Build a Language](../build-a-language/index.md)
