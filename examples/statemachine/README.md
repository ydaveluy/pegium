# Statemachine Example (Pegium/C++)

This example is implemented in Pegium/C++.

Full documentation:

- [Published docs](https://ydaveluy.github.io/pegium/examples/statemachine/)
- [Docs source](../../docs/examples/statemachine.md)

It provides:

- A parser for the `.statemachine` language.
- A stdio LSP server (`JSON-RPC`) for the `statemachine` language id.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run CLI

```bash
./build/examples/statemachine/pegium-example-statemachine-cli \
  examples/statemachine/example/trafficlight.statemachine
```

## Run LSP server (stdio)

```bash
./build/examples/statemachine/pegium-example-statemachine-lsp
```

## VSCode extension

### 1) Build the C++ server

```bash
cmake -S . -B build
cmake --build build -j
```

### 2) Install and compile extension side

```bash
cd examples/statemachine
npm install
npm run compile
```

### 3) Run in VSCode

- Open `examples/statemachine` as the workspace folder in VSCode.
- Start `Run Statemachine Extension` from `.vscode/launch.json`.

Or from the repository root, press `F5` and select `Run Statemachine Extension`.

If the server binary is not found automatically, set:

- setting `pegium.statemachine.serverPath`
or
- env var `PEGIUM_STATEMACHINE_SERVER`
