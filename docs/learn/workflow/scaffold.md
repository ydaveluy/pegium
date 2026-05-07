# 2. Choose a Starting Point

The recommended way to start a new Pegium language is the
[**`pegium-language-template`**](https://github.com/ydaveluy/pegium-language-template)
GitHub template repository. It ships a working "Hello world" DSL,
pulls Pegium in via `FetchContent`, and includes a one-shot CMake
script that renames every identifier to your own.

## What the template ships

A working DSL for a tiny grammar that declares persons and greets
them by name:

```text
person John
person Jane

Hello John!
Hello Jane!
```

You get:

- `src/mydsl/` — AST, parser (grammar), core/LSP services
- `example/sample.mydsl` — sample input the smoke test parses
- `cmake/pegium.cmake` — `FetchContent_Declare(pegium ...)` (one place
  to bump the pinned tag)
- `scripts/new-language.cmake` — the rename script (no extra runtime,
  pure CMake)
- `tests/ParseSmoke.cpp` — CTest smoke test
- `.vscode/launch.json` + `tasks.json` — F5 builds and runs the CLI on
  the sample

## Bootstrap your project

On
[GitHub](https://github.com/ydaveluy/pegium-language-template),
click **Use this template → Create a new repository** to create a copy
under your own account. Then clone it locally:

```bash
git clone https://github.com/<your-account>/<your-repo>.git my-language
cd my-language
```

Rename the default identifiers (`mydsl`, `MyDsl`, `MYDSL`, `.mydsl`) to
your own:

```bash
cmake -P scripts/new-language.cmake -DLANGUAGE_NAME=mylang
```

Optional flags:

- `-DLANGUAGE_CLASS=MyLang` — PascalCase for class names. Defaults to
  the lowercase name with the first letter capitalized.
- `-DFILE_EXT=.ml` — file extension. Defaults to `.${LANGUAGE_NAME}`.

Then build and test:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/mylang-cli example/sample.mylang
```

The first build pulls Pegium via `FetchContent` and produces a CLI and
an LSP server for your language. Open the folder in VS Code and press
**F5** to debug the CLI on the sample.

## What to edit next

| File                                    | Role                                       |
|-----------------------------------------|--------------------------------------------|
| `src/mylang/parser/Parser.hpp`          | grammar rules and terminals                |
| `src/mylang/ast.hpp`                    | AST types and reference fields             |
| `src/mylang/core/Module.cpp`            | core service wiring (parser, validator…)   |
| `src/mylang/lsp/Module.cpp`             | LSP feature wiring (formatter, hover…)     |
| `example/sample.mylang`                 | sample input the smoke test parses         |
| `cmake/pegium.cmake`                    | the Pegium tag this project pins           |

## Recommended next step

Once you have a renamed project that builds, continue with
[Write the Grammar](write_grammar.md).
