# Requirements Example (Pegium/C++)

This example is implemented in Pegium/C++.

Full documentation:

- [Published docs](https://ydaveluy.github.io/pegium/examples/requirements/)
- [Docs source](../../docs/examples/requirements.md)

It provides:

- A parser for `.req` (`requirements-lang`).
- A parser for `.tst` (`tests-lang`).
- A single stdio LSP server (`JSON-RPC`) registering both language ids.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run CLI

```bash
./build/examples/requirements/pegium-example-requirements-cli \
  examples/requirements/example/requirements.req

./build/examples/requirements/pegium-example-requirements-cli \
  examples/requirements/example/tests_part1.tst
```

## Run LSP server (stdio)

```bash
./build/examples/requirements/pegium-example-requirements-lsp
```

## VSCode extension

### 1) Build the C++ server

```bash
cmake -S . -B build
cmake --build build -j
```

### 2) Install and compile extension side

```bash
cd examples/requirements
npm install
npm run compile
```

### 3) Run in VSCode

- Open `examples/requirements` as the workspace folder in VSCode.
- Start `Run Requirements Extension` from `.vscode/launch.json`.

Or from the repository root, press `F5` and select `Run Requirements Extension`.

If the server binary is not found automatically, set:

- setting `pegium.requirements.serverPath`
or
- env var `PEGIUM_REQUIREMENTS_SERVER`
