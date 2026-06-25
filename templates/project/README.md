# @PEGIUM_NEW_LANGUAGE_ID@

A hello-world language built with [pegium](https://github.com/ydaveluy/pegium).

## Quick start

### Build

```bash
cmake -B build
cmake --build build -j
```

### Run the CLI

```bash
./build/@PEGIUM_NEW_LANGUAGE_ID@-cli example/hello@PEGIUM_NEW_EXT@
```

### Open it in VS Code (press F5)

The recommended way to try the language is in the editor. Open this folder in VS
Code and press **F5** — that's the whole setup.

The first launch runs everything for you: it configures and builds the C++
language server, installs the extension's npm dependencies, and compiles the
TypeScript. Then a second window (the Extension Development Host) opens on the
`example/` folder with `hello@PEGIUM_NEW_EXT@` already open, so live syntax
highlighting, diagnostics and the other LSP features are active immediately.

> The first F5 is slow: CMake clones and builds pegium via `FetchContent`.
> Later launches are incremental.
>
> If the Extension Development Host shows *"Unable to locate
> @PEGIUM_NEW_LANGUAGE_ID@-lsp"*, the C++ build failed — check the **Terminal**
> and **Problems** panels in the main window.

### Run the tests

```bash
ctest --test-dir build --output-on-failure
```

These also run in CI on every push and pull request — see
[`.github/workflows/ci.yml`](.github/workflows/ci.yml).

### Package & publish the extension

`npm run package` builds the C++ server **Release and stripped** (in a dedicated
`build-release/` tree, so it doesn't disturb the Debug build F5 uses), copies it
into the extension's `bin/`, and produces a **platform-specific** `.vsix` — the
TypeScript is bundled with [esbuild](https://esbuild.github.io/) and the small,
optimized native server is bundled in `bin/`, so the result works as soon as it
is installed, with nothing else to configure:

```bash
(cd vscode && npm install && npm run package)          # for your platform
(cd vscode && npm run package -- --target linux-x64)   # for a specific target
```

Because the C++ server cannot be cross-compiled easily, each platform's `.vsix`
is normally built on that platform. The included
[`.github/workflows/release.yml`](.github/workflows/release.yml) does exactly that
— a Linux/Windows/macOS matrix builds and uploads one `.vsix` per target on every
`v*` tag.

To publish to the Marketplace, set a real `publisher` in `vscode/package.json`,
[create a publisher and a Personal Access Token](https://code.visualstudio.com/api/working-with-extensions/publishing-extension),
then add a `VSCE_PAT` secret and uncomment the publish step in the workflow (or run
`VSCE_PAT=… npm run publish` locally).

## What to edit next

| What you want to change     | File(s) to edit                                       |
|-----------------------------|-------------------------------------------------------|
| Add a new AST node          | `src/@PEGIUM_NEW_LANGUAGE_ID@/core/ast.hpp`                                  |
| Extend the grammar          | `src/@PEGIUM_NEW_LANGUAGE_ID@/core/@PEGIUM_NEW_CLASS@Parser.hpp`                        |
| Add validation checks       | `src/@PEGIUM_NEW_LANGUAGE_ID@/core/` (add a validator class)            |
| Add LSP features (hover, …) | `src/@PEGIUM_NEW_LANGUAGE_ID@/lsp/`                                     |
| Add a code generator        | `src/@PEGIUM_NEW_LANGUAGE_ID@/cli/main.cpp`            |
| Add more test cases         | `test/parsing_test.cpp`                               |

## Grammar at a glance

```
person John
person Jane

Hello John!
Hello Jane!
```

The grammar defines two kinds of declarations:

- `person <Name>` — declares a person.
- `Hello <Name>!` — greets a previously declared person.
