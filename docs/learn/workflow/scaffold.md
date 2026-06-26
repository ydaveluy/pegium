# Create a Project

Scaffold a new Pegium language with a single command — no cloning or GitHub
account required:

```bash
curl -fsSLO https://ydaveluy.github.io/pegium/pegium-new.cmake && \
  cmake -DNAME=MyLang -DEXT=.ml -P pegium-new.cmake
cd mylang && cmake -B build && cmake --build build -j
./build/mylang-cli example/hello.ml
```

The script creates a `mylang/` directory, pulls Pegium in via `FetchContent`,
and gives you a working "Hello world" grammar, CLI, LSP server, and VS Code
extension in one step. No extra runtime beyond CMake is required to scaffold.
Node.js is only needed if you keep the VS Code extension enabled (the default).

!!! tip "Prefer to start from an existing example?"
    The four working examples under `examples/` demonstrate every concept in
    this workflow, so copying the closest one is a fully in-repo, verifiable
    starting point. `statemachine` is the smallest complete language (AST +
    grammar + validation + formatter); `arithmetics` is the smallest full
    parser-to-editor path. See [Examples](../../examples/index.md).

## What the scaffolder creates

A working DSL for a tiny grammar that declares persons and greets them by name:

```text
person John
person Jane

Hello John!
Hello Jane!
```

You get:

- `src/mylang/` — AST, parser (grammar), core services, LSP services
- `example/hello.<ext>` — sample input the smoke test parses
- `CMakeLists.txt` — `FetchContent_Declare(pegium ...)` (one place to bump
  the pinned tag); sources are **globbed**, so adding a `.cpp` under `src/` or a
  test under `test/` needs no edit here
- `test/parsing_test.cpp` — a CTest smoke test (core pipeline). With the LSP
  server, `test/lsp/` adds feature tests built on the reusable
  [`pegium::testing`](../test-your-language.md#testing-lsp-features-with-pegiumtesting)
  harness, already linked for you
- `.vscode/launch.json` — F5 launches the language extension in an Extension Development Host
- `vscode/` — VS Code extension (omitted when `-DVSCODE=OFF`)

## Scaffolding flags

| Flag | Default | Description |
|------|---------|-------------|
| `NAME` | *(required)* | PascalCase C++ identifier for your language (e.g. `MyLang`) |
| `EXT` | `.<lowercased-name>` | File extension, must start with `.` (e.g. `-DEXT=.ml`) |
| `DIR` | `<lowercased-name>` | Output directory (e.g. `-DDIR=my-project`) |
| `LSP` | `ON` | Build the LSP server; pass `-DLSP=OFF` to skip |
| `VSCODE` | `ON` | Scaffold the VS Code extension; pass `-DVSCODE=OFF` to skip |
| `CLI` | `ON` | Build the CLI tool; pass `-DCLI=OFF` to skip |
| `PEGIUM_TAG` | `main` | Pegium tag/commit to pin (e.g. `-DPEGIUM_TAG=v1.2.0`) |

## What to edit next

| File                                    | Role                                       |
|-----------------------------------------|--------------------------------------------|
| `src/mylang/core/MyLangParser.hpp`          | grammar rules and terminals                |
| `src/mylang/core/ast.hpp`                    | AST types and reference fields             |
| `src/mylang/core/CoreModule.cpp`            | core service wiring (parser, validator…)   |
| `src/mylang/lsp/LspModule.cpp`             | LSP feature wiring (formatter, hover…)     |
| `example/hello.<ext>`                   | sample input the smoke test parses         |
| `test/`                                 | add tests (globbed — new files need no CMake change) |
| `CMakeLists.txt`                        | the Pegium tag this project pins           |

## Related pages

- [End-to-end walkthrough](../walkthrough.md) — fill in the grammar, AST,
  validation, and services
- [Examples](../../examples/index.md) — four working in-repo languages
