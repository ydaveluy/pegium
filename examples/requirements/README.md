# Requirements

This example demonstrates how to register and serve multiple related languages
inside one Pegium workspace.

It is the best starting point when your real project spans several grammars or
file types that should still share one document and indexing model.

## What it shows

- a parser for `.req` files (`requirements-lang`)
- a parser for `.tst` files (`tests-lang`)
- one shared workspace and one shared stdio LSP server
- cross-file and cross-language references
- service registration for multiple language ids

## Use this example when

- several file types belong to one logical workspace
- references cross file or language boundaries
- you need a realistic multi-language service setup
- you want a concrete example for `registerServices(...)`

## Build

From the repository root:

```sh
cmake -S . -B build
cmake --build build -j
```

## Run the CLI

```sh
./build/examples/requirements/pegium-example-requirements-cli \
  examples/requirements/example/requirements.req

./build/examples/requirements/pegium-example-requirements-cli \
  examples/requirements/example/tests_part1.tst
```

## Run the LSP server

```sh
./build/examples/requirements/pegium-example-requirements-lsp
```

## Run the VS Code client

### 1. Build the C++ server

```sh
cmake -S . -B build
cmake --build build -j
```

### 2. Install and compile the extension side

```sh
cd examples/requirements
npm install
npm run compile
```

### 3. Start the extension in VS Code

- Open `examples/requirements` as the workspace folder in VS Code.
- Start `Run Requirements Extension` from `.vscode/launch.json`.

You can also open the repository root in VS Code, press `F5`, and select
`Run Requirements Extension`.

If the server binary is not found automatically, set either:

- the `pegium.requirements.serverPath` setting
- the `PEGIUM_REQUIREMENTS_SERVER` environment variable

## Where to go next

- [Examples Overview](../../docs/examples/index.md)
- [Learn Pegium](../../docs/learn/index.md)
- [Requirements example page](../../docs/examples/requirements.md)
