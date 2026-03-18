# Statemachine

This example is a compact modeling language centered on semantic validation and
editor integration.

It is a good starting point when your language is built around connected model
elements and domain rules rather than expression evaluation.

## What it shows

- a parser for the `.statemachine` language
- semantic validation rules
- formatting and LSP integration
- a stdio LSP server for the `statemachine` language id
- a VS Code client for interactive testing

## Use this example when

- semantic validation is central to the language
- the domain is graph- or state-based
- you want a compact modeling example with editor support
- you want to study validation-oriented services

## Build

From the repository root:

```sh
cmake -S . -B build
cmake --build build -j
```

## Run the CLI

```sh
./build/examples/statemachine/pegium-example-statemachine-cli \
  examples/statemachine/example/trafficlight.statemachine
```

## Run the LSP server

```sh
./build/examples/statemachine/pegium-example-statemachine-lsp
```

## Run the VS Code client

### 1. Build the C++ server

```sh
cmake -S . -B build
cmake --build build -j
```

### 2. Install and compile the extension side

```sh
cd examples/statemachine
npm install
npm run compile
```

### 3. Start the extension in VS Code

- Open `examples/statemachine` as the workspace folder in VS Code.
- Start `Run Statemachine Extension` from `.vscode/launch.json`.

You can also open the repository root in VS Code, press `F5`, and select
`Run Statemachine Extension`.

If the server binary is not found automatically, set either:

- the `pegium.statemachine.serverPath` setting
- the `PEGIUM_STATEMACHINE_SERVER` environment variable

## Where to go next

- [Examples Overview](../../docs/examples/index.md)
- [Learn Pegium](../../docs/learn/index.md)
- [Statemachine example page](../../docs/examples/statemachine.md)
