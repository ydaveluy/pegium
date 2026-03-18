# DomainModel

This example shows a structure-first modeling DSL with declarations, nesting,
qualified names, formatting, and rename support.

It is a strong starting point when your target language looks more like a
modeling language than an expression language.

## What it shows

- a parser for the `.dmodel` language
- packages, entities, datatypes, and structured AST nodes
- qualified-name-based references
- formatter rules for block-oriented syntax
- a stdio LSP server for the `domain-model` language id

## Use this example when

- the language has declarations, nesting, and blocks
- qualified names matter
- you want a clear reference and rename example
- you want a formatter centered on keywords and brace blocks

## Build

From the repository root:

```sh
cmake -S . -B build
cmake --build build -j
```

## Run the CLI

```sh
./build/examples/domainmodel/pegium-example-domainmodel-cli \
  examples/domainmodel/example/blog.dmodel
```

## Run the LSP server

```sh
./build/examples/domainmodel/pegium-example-domainmodel-lsp
```

## Run the VS Code client

### 1. Build the C++ server

```sh
cmake -S . -B build
cmake --build build -j
```

### 2. Install and compile the extension side

```sh
cd examples/domainmodel
npm install
npm run compile
```

### 3. Start the extension in VS Code

- Open `examples/domainmodel` as the workspace folder in VS Code.
- Start `Run DomainModel Extension` from `.vscode/launch.json`.

You can also open the repository root in VS Code, press `F5`, and select
`Run DomainModel Extension`.

If the server binary is not found automatically, set either:

- the `pegium.domainmodel.serverPath` setting
- the `PEGIUM_DOMAINMODEL_SERVER` environment variable

## Where to go next

- [Examples Overview](../../docs/examples/index.md)
- [Learn Pegium](../../docs/learn/index.md)
- [DomainModel example page](../../docs/examples/domainmodel.md)
