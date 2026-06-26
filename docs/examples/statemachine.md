# Statemachine

Source: [examples/statemachine](https://github.com/ydaveluy/pegium/tree/main/examples/statemachine)

A compact modeling example where semantic rules matter more than expression parsing. Reach for it when you want to study whole-model validation and editor support together.

## What it shows

- a state-oriented modeling language
- semantic validation rules
- formatting and LSP integration
- a VS Code client for interactive testing

## Useful entry points

- CLI: `./build/examples/statemachine/pegium-example-statemachine-cli generate examples/statemachine/example/trafficlight.statemachine`
- LSP: `./build/examples/statemachine/pegium-example-statemachine-lsp`

## What to read first

- `examples/statemachine/src/statemachine/core/CoreModule.cpp` for the service setup
- `examples/statemachine/src/statemachine/core/validation/StatemachineValidator.cpp` for typed validation checks
- `examples/statemachine/src/statemachine/lsp/StatemachineFormatter.cpp` for the formatter

## Use this example when

- semantic validation is central to the language
- the domain is graph- or state-based
- your language centers on connected model elements and validation rules rather than expression evaluation

## Related pages

- [Validation](../recipes/validation/index.md)
- [Custom Validator](../recipes/custom-validator.md)
- [Build a Language](../build-a-language/index.md)
