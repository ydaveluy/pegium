# 2. Choose a Starting Point

Pegium does not ask you to start from an empty directory. There are two
working starting points, depending on whether you are building your own
language or contributing one back to the framework.

| You want to… | Start from |
|---|---|
| Build a language as its own project, with Pegium as a dependency | the **language template** ([`template/`](https://github.com/ydaveluy/pegium/tree/main/template) in this repo) |
| Add a new shipped example inside the Pegium repository | one of the **shipped examples** under `examples/` |

If you are not sure, pick the template path. It is what most users want.

## Path A — start from the language template

The template is a complete "Hello world" Pegium project that builds
out of the box, pulls Pegium in via `FetchContent`, and ships a
one-shot CMake script that renames every identifier to your own.

### What the template ships

A working DSL for a tiny grammar inspired by Langium's HelloWorld:

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

### Bootstrap your project from the template

Until the template ships as its own GitHub-template repository, copy
the `template/` directory out of the Pegium repo:

```bash
cp -r pegium/template my-language
cd my-language
git init
```

Then rename the default identifiers (`mydsl`, `MyDsl`, `MYDSL`,
`.mydsl`) to your own:

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

### What to edit next

- `src/mylang/parser/Parser.hpp` — your grammar
- `src/mylang/ast.hpp` — AST types and reference fields
- `src/mylang/core/Module.cpp` — service wiring (parser, validator…)
- `src/mylang/lsp/Module.cpp` — LSP feature wiring (formatter, hover…)
- `cmake/pegium.cmake` — bump the Pegium tag here when you upgrade

## Path B — start from a shipped example

This path is for contributing a language back to Pegium. Use it when
your language is going to live inside `examples/` of this repository,
sharing the build system and CI with the framework.

### Which example to copy

- [arithmetics](../../examples/arithmetics.md) — smallest complete
  parser-to-editor example
- [domainmodel](../../examples/domainmodel.md) — nested declarations
  and rename
- [requirements](../../examples/requirements.md) — multi-language and
  workspace behavior
- [statemachine](../../examples/statemachine.md) — validation-heavy
  modeling DSLs

If you are not sure, copy `arithmetics`.

### Concrete renaming steps

The example layout uses a per-language namespace folder, so renaming is
mechanical. Suppose your language is named `mylang`:

1. Copy the example folder, for instance
   `examples/arithmetics` → `examples/mylang`.
2. Rename the inner namespace folder
   `examples/mylang/src/arithmetics/` → `examples/mylang/src/mylang/`.
3. In `examples/mylang/CMakeLists.txt`, replace every occurrence of
   `arithmetics` with `mylang` (target names, install paths,
   `add_subdirectory(...)`, include directories).
4. In every C++ source under the new tree, replace:
   - the namespace `arithmetics::...` with `mylang::...`
   - includes `<arithmetics/...>` with `<mylang/...>`
   - the parser class name `ArithmeticsParser` with `MylangParser`
   - the file extension `.calc` (in `Module.cpp`) with your own
5. Add the new example folder to the top-level `examples/CMakeLists.txt`.
6. Rebuild: `cmake --build build -j`.

At this point you should have a binary like
`build/examples/mylang/pegium-example-mylang-cli` that still parses
the arithmetics grammar but lives entirely under your own names.

### What to keep for the first iteration

While you adapt the example, leave these pieces alone until the
language is parsing again:

- the overall project layout
- the service bootstrap (`Module.cpp`, `Services.hpp`)
- the CLI and LSP entrypoints (`cli_main.cpp`, `lsp_main.cpp`)
- the test structure

Once your renamed copy still builds and runs, you can start replacing
the grammar, AST, and services step by step.

## Recommended next step

Once you have a renamed project that still builds, continue with
[Write the Grammar](write_grammar.md).
