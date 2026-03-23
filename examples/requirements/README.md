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
cmake --build build
```

## Run the CLI

```sh
./build/examples/requirements/pegium-example-requirements-cli \
  generate examples/requirements/example/requirements.req
```

## Run the LSP server

```sh
./build/examples/requirements/pegium-example-requirements-lsp
```

## Run the VS Code client

### 1. Build the C++ server

```sh
cmake -S . -B build
cmake --build build
```

### 2. Install and compile the extension side

```sh
cd examples/requirements
npm install
npm run compile
```

### 3. Start the extension in VS Code

- Open the repository root in VS Code.
- Go to `Run and Debug`.
- Start `Run Requirements Extension` with `F5`.

The launch configuration lives in the repository root
`.vscode/launch.json` and opens an Extension Development Host on
`examples/requirements/example`.

If the server binary is not found automatically, set either:

- the `pegium.requirements.serverPath` setting
- the `PEGIUM_REQUIREMENTS_SERVER` environment variable

## Where to go next

- [Examples Overview](../../docs/examples/index.md)
- [Learn Pegium](../../docs/learn/index.md)
- [Requirements example page](../../docs/examples/requirements.md)
