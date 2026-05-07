# Pegium Language Template

A working "Hello world" DSL built with [Pegium](https://github.com/ydaveluy/pegium).
Use it as the starting point for your own language.

## What you get

A complete C++ project that:

- builds against Pegium pulled in via `FetchContent` — no manual install
- ships a tiny grammar (`Person` declarations + `Greeting` cross-references)
  inspired by Langium's HelloWorld example
- includes a CLI that parses a file, links cross-references, and prints a
  short summary
- includes an LSP server with the default Pegium feature set
- runs a smoke test on the bundled sample to prove end-to-end behavior

```text
person John
person Jane

Hello John!
Hello Jane!
```

is valid input out of the box, and the smoke test asserts both greetings
resolve to a real `Person`.

## Quickstart (with the default name `mydsl`)

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure   # runs the smoke test
./build/mydsl-cli example/sample.mydsl
```

Expected output:

```text
Parsed 2 person(s) and 2 greeting(s).
  Hello -> John
  Hello -> Jane
```

## Make it your own

A one-shot rename gives every identifier, namespace, file, folder, target,
and file extension your own name:

```bash
cmake -P scripts/new-language.cmake -DLANGUAGE_NAME=mylang
```

Optional flags:

- `-DLANGUAGE_CLASS=MyLang` — PascalCase used in class names. Defaults to
  the lowercase name with the first letter capitalized.
- `-DFILE_EXT=.ml` — file extension. Defaults to `.${LANGUAGE_NAME}`.

After the rename, repeat the build:

```bash
cmake -S . -B build
cmake --build build -j
./build/mylang-cli example/sample.mylang
```

The script edits files in place. Run it once on a fresh checkout.

## Where to edit next

| File                                | What lives there                              |
|-------------------------------------|-----------------------------------------------|
| `src/mydsl/parser/Parser.hpp`       | grammar rules and terminals                   |
| `src/mydsl/ast.hpp`                 | AST types and reference fields                |
| `src/mydsl/core/Module.cpp`         | core service wiring (parser, validator, etc.) |
| `src/mydsl/lsp/Module.cpp`          | LSP feature wiring (formatter, hover, etc.)   |
| `example/sample.mydsl`              | sample input the smoke test parses            |
| `cmake/pegium.cmake`                | the Pegium tag this project pins              |

## VS Code

The repo ships `.vscode/launch.json` with two debug targets:

- **Run MyDsl CLI on sample** — builds and launches `mydsl-cli` on
  `example/sample.mydsl`. Press **F5** after opening the folder.
- **Debug MyDsl LSP** — launches `mydsl-lsp` on stdio so you can attach a
  client.

Both depend on the `Build MyDsl` task in `.vscode/tasks.json`.

## Pegium documentation

- [Pegium docs](https://ydaveluy.github.io/pegium/)
- [Grammar reference](https://ydaveluy.github.io/pegium/reference/grammar-reference/)
- [Examples](https://ydaveluy.github.io/pegium/examples/)
