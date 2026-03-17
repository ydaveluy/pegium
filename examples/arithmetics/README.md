# Arithmetics Example (Pegium/C++)

This example is implemented in Pegium/C++.

Full documentation:

- [Published docs](https://ydaveluy.github.io/pegium/examples/arithmetics/)
- [Docs source](../../docs/examples/arithmetics.md)

It provides:

- A parser for the `.calc` language.
- A small interpreter for arithmetic evaluations.
- A stdio LSP server (`JSON-RPC`) for `arithmetics` / `calc` language ids.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run CLI

```bash
./build/examples/arithmetics/pegium-example-arithmetics-cli \
  examples/arithmetics/example/example.calc
```

## Run LSP server (stdio)

```bash
./build/examples/arithmetics/pegium-example-arithmetics-lsp
```

## VSCode extension

The folder now contains a `package.json` + `vscode/src/extension.ts` client that
starts the Pegium server executable (`pegium-example-arithmetics-lsp`).

### 1) Build the C++ server

```bash
cmake -S . -B build
cmake --build build -j
```

### 2) Install and compile extension side

```bash
cd examples/arithmetics
npm install
npm run compile
```

### 3) Run in VSCode

- Open `examples/arithmetics` as the workspace folder in VSCode.
- Start `Run Arithmetics Extension` from `.vscode/launch.json`.

Or from the repository root, simply press `F5` and select
`Run Arithmetics Extension` (root `.vscode/launch.json`).

If the server binary is not found automatically, set:

- setting `pegium.arithmetics.serverPath`
or
- env var `PEGIUM_ARITHMETICS_SERVER`
