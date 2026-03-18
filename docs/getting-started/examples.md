# Run the Examples

Pegium ships four examples that cover different language shapes and services.

This page is kept as a focused companion to the newer
[Examples Overview](../examples/index.md) and [Learn Pegium](../learn/index.md)
sections.

The examples are not toy fragments. Each of them is a useful template for a
different kind of language project.

## Command-line entry points

```bash
./build/examples/arithmetics/pegium-example-arithmetics-cli \
  examples/arithmetics/example/example.calc

./build/examples/domainmodel/pegium-example-domainmodel-cli \
  examples/domainmodel/example/blog.dmodel

./build/examples/requirements/pegium-example-requirements-cli \
  examples/requirements/example/requirements.req

./build/examples/statemachine/pegium-example-statemachine-cli \
  examples/statemachine/example/trafficlight.statemachine
```

## LSP server entry points

```bash
./build/examples/arithmetics/pegium-example-arithmetics-lsp
./build/examples/domainmodel/pegium-example-domainmodel-lsp
./build/examples/requirements/pegium-example-requirements-lsp
./build/examples/statemachine/pegium-example-statemachine-lsp
```

## VS Code clients

Each example contains a small VS Code client under `examples/<name>/vscode/`.
Build the example server first, then install the Node dependencies in the
example directory.

Typical flow:

```bash
cd examples/arithmetics
npm install
npm run compile
```

Then open the example folder in VS Code and start the matching launch
configuration.

## Pick your starting point

- Start from `arithmetics` for a compact parser + evaluator + formatter flow.
- Start from `domainmodel` for a structure-first modeling language.
- Start from `requirements` for multi-language workspace behavior.
- Start from `statemachine` for state-oriented modeling and validation.

## Recommended reading after running one example

- [Your first language](first-language.md)
- [Workflow](../learn/workflow/index.md)
- [Examples overview](../examples/index.md)
