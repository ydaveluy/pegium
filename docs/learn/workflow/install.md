# 1. Build the Repository

There are two reasons to clone and build Pegium itself:

1. **You want to verify the framework runs on your machine** before you
   commit to using it. Run one of the shipped examples end-to-end to
   confirm.
2. **You want to contribute to Pegium itself** — fix a bug, add a
   feature, ship a new example.

If your goal is to **build your own language**, you do not need to
clone Pegium at all. Skip to
[2. Choose a Starting Point](scaffold.md) and start from the language
template — Pegium gets pulled in as a `FetchContent` dependency.

## What you need

- a C++20-capable compiler
- CMake 3.14 or later
- Ninja or another generator supported by your setup
- Node.js only if you want to run the VS Code clients shipped in the examples

## Build the repository

Clone the repository and build it:

```bash
git clone https://github.com/ydaveluy/pegium.git
cd pegium
cmake -S . -B build
cmake --build build -j
```

A successful build produces:

- the framework libraries under `build/src/pegium/`
- one CLI and one LSP binary per shipped example under
  `build/examples/<language>/`

Try one of the example CLIs to confirm everything works end-to-end:

```bash
./build/examples/arithmetics/pegium-example-arithmetics-cli \
  eval examples/arithmetics/example/example.calc
```

If you also want to validate the test suite:

```bash
ctest --test-dir build --output-on-failure
```

You can also run the main test groups independently:

```bash
ctest --test-dir build -L core --output-on-failure
ctest --test-dir build -L lsp --output-on-failure
ctest --test-dir build -L integration --output-on-failure
ctest --test-dir build -L example --output-on-failure
```

## Run the fuzz target

Pegium also ships a FuzzTest-based parser stress target:

```bash
CC=clang CXX=clang++ cmake -S . -B build-fuzz-ci \
  -DBUILD_TESTING=ON \
  -DPEGIUM_BUILD_EXAMPLES=OFF \
  -DPEGIUM_BUILD_FUZZ_TARGETS=ON \
  -DFUZZTEST_FUZZING_MODE=ON
cmake --build build-fuzz-ci --target PegiumWorkspaceFuzzTest -j32
```

For CMake consumers, Pegium exposes these target aliases:

- `pegium::core` for the parser, workspace, references and validation runtime
- `pegium::converters` for grammar and AST conversion helpers
- `pegium::lsp` for the LSP-enabled aggregate library
- `pegium::cli` for the standalone CLI support layer built on top of `pegium::core`

Pegium is meant to be consumed as a subproject via `FetchContent`. The
[language template](scaffold.md) already wires this up; the snippet
below is the underlying machinery in case you need to integrate Pegium
into an existing CMake project:

```cmake
include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "")
set(PEGIUM_BUILD_EXAMPLES OFF CACHE BOOL "")
# Optional: let Pegium build shared libraries as well.
set(BUILD_SHARED_LIBS ON CACHE BOOL "")

FetchContent_Declare(
  pegium
  GIT_REPOSITORY https://github.com/ydaveluy/pegium.git
  GIT_TAG <release-tag-or-immutable-commit>
)
FetchContent_MakeAvailable(pegium)

target_link_libraries(your_target PRIVATE pegium::core)
```

When Pegium is used as a subproject, the public targets remain available, but
they are declared `EXCLUDE_FROM_ALL`. In practice, `cmake --build` only builds
the Pegium libraries that your own targets actually reach through
`target_link_libraries(...)`.

## Repository layout at a glance

The main directories to keep in mind are:

- `src/pegium/` for the framework
- `examples/` for end-to-end languages
- `tests/` for usage patterns and regression coverage
- `docs/` for this documentation

## Outcome

At the end of this step, you should have a working build and at least one
example binary that you can run locally.

## Continue with

- [2. Choose a Starting Point](scaffold.md)
- [Examples Overview](../../examples/index.md)
