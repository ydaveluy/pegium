# Arithmetics

This example is the smallest end-to-end Pegium language in the repository.

It is a good starting point when you want to understand the full path from
grammar to CLI and language server without first dealing with a large semantic
model.

## What it shows

- a parser for the `.calc` language
- an evaluator for arithmetic expressions
- validation and formatting
- a stdio LSP server for `arithmetics`
- a VS Code client for interactive testing

## Use this example when

- your language is expression-heavy
- you need precedence and associativity
- you want to study `Infix` rules
- you want the most compact full-stack Pegium example

## Build

From the repository root:

```sh
cmake -S . -B build
cmake --build build -j
```

## Run the CLI

```sh
./build/examples/arithmetics/pegium-example-arithmetics-cli \
  eval examples/arithmetics/example/example.calc
```

## Run the LSP server

```sh
./build/examples/arithmetics/pegium-example-arithmetics-lsp
```

## Run the VS Code client

### 1. Build the C++ server

```sh
cmake -S . -B build
cmake --build build -j
```

### 2. Install and compile the extension side

```sh
cd examples/arithmetics
npm install
npm run compile
```

### 3. Start the extension in VS Code

- Open `examples/arithmetics` as the workspace folder in VS Code.
- Start `Run Arithmetics Extension` from `.vscode/launch.json`.

You can also open the repository root in VS Code, press `F5`, and select
`Run Arithmetics Extension`.

If the server binary is not found automatically, set either:

- the `pegium.arithmetics.serverPath` setting
- the `PEGIUM_ARITHMETICS_SERVER` environment variable

## Where to go next

- [Examples Overview](../../docs/examples/index.md)
- [Learn Pegium](../../docs/learn/index.md)
- [Arithmetics example page](../../docs/examples/arithmetics.md)
