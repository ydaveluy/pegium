# Requirements

Source: [examples/requirements](https://github.com/ydaveluy/pegium/tree/main/examples/requirements)

`requirements` is the example to study once one language is no longer enough.
It shows how Pegium behaves when several related file types belong to the same
workspace and are served by the same runtime.

## What it shows

- two related languages served by one workspace
- cross-file and cross-language behavior
- references and scoping beyond a single document
- a shared LSP server for `.req` and `.tst`

## Useful entry points

- CLI: `./build/examples/requirements/pegium-example-requirements-cli`
- LSP: `./build/examples/requirements/pegium-example-requirements-lsp`

## What to read first

The clearest reading path is:

- `examples/requirements/src/RequirementsModule.cpp` for multi-language
  registration
- `examples/requirements/src/Language.cpp` for the language definitions
- `examples/requirements/src/validation/RequirementsValidator.cpp` and
  `examples/requirements/src/validation/TestsValidator.cpp` for
  language-specific validation on both sides of the workspace
- `examples/requirements/src/lsp/RequirementsFormatter.cpp` for shared
  editor-facing behavior

## Why start here

Use this example when your real project spans several related grammars or file
types.

## Use this example when

- several file types belong to one logical workspace
- references cross file or language boundaries
- you need a more realistic document/index pipeline than a single-file example

## Continue with

- [Multiple Languages](../recipes/multiple-languages.md)
- [Workspace Lifecycle](../build-a-language/workspace.md)
- [Document Lifecycle](../reference/document-lifecycle.md)
