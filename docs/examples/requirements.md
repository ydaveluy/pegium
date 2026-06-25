# Requirements

Source: [examples/requirements](https://github.com/ydaveluy/pegium/tree/main/examples/requirements)

Study `requirements` once one language is no longer enough. It shows how Pegium serves several related file types from one workspace and runtime: two languages, cross-file and cross-language references, scoping beyond a single document, and a shared LSP server for `.req` and `.tst`.

## Entry points

- CLI: `./build/examples/requirements/pegium-example-requirements-cli generate examples/requirements/example/requirements.req`
- LSP: `./build/examples/requirements/pegium-example-requirements-lsp`

## What to read first

- `examples/requirements/src/requirements/core/Module.cpp` — multi-language registration
- `examples/requirements/src/requirements/core/Language.cpp` — the language definitions
- `examples/requirements/src/requirements/core/validation/RequirementsValidator.cpp` and `examples/requirements/src/requirements/core/validation/TestsValidator.cpp` — language-specific validation on both sides of the workspace
- `examples/requirements/src/requirements/lsp/RequirementsFormatter.cpp` — shared editor-facing behavior

## Use this example when

- Several file types belong to one logical workspace.
- References cross file or language boundaries.
- You need a more realistic document/index pipeline than a single-file example.

## Related pages

- [Multiple Languages](../recipes/multiple-languages.md)
- [Workspace Lifecycle](../build-a-language/workspace.md)
- [Document Lifecycle](../reference/document-lifecycle.md)
