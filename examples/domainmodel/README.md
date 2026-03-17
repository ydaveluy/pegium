# Domainmodel Example (Pegium/C++)

This example is implemented in Pegium/C++.

Full documentation:

- [Published docs](https://ydaveluy.github.io/pegium/examples/domainmodel/)
- [Docs source](../../docs/examples/domainmodel.md)

It provides:

- A parser for the `.dmodel` language.
- A stdio LSP server (`JSON-RPC`) for the `domain-model` language id.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run CLI

```bash
./build/examples/domainmodel/pegium-example-domainmodel-cli \
  examples/domainmodel/example/blog.dmodel
```

## Run LSP server (stdio)

```bash
./build/examples/domainmodel/pegium-example-domainmodel-lsp
```

## VSCode extension

### 1) Build the C++ server

```bash
cmake -S . -B build
cmake --build build -j
```

### 2) Install and compile extension side

```bash
cd examples/domainmodel
npm install
npm run compile
```

### 3) Run in VSCode

- Open `examples/domainmodel` as the workspace folder in VSCode.
- Start `Run DomainModel Extension` from `.vscode/launch.json`.

Or from the repository root, press `F5` and select `Run Domainmodel Extension`.

If the server binary is not found automatically, set:

- setting `pegium.domainmodel.serverPath`
or
- env var `PEGIUM_DOMAINMODEL_SERVER`
