# 1. Build the Repository

Before designing your own language, make sure Pegium itself builds on your
machine and that you can run at least one shipped example.

## What you need

- a C++20-capable compiler
- CMake
- Ninja or another generator supported by your setup
- Node.js only if you want to run the VS Code clients shipped in the examples

## Build the repository

```bash
cmake -S . -B build
cmake --build build -j32
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

Pegium is also meant to be consumed as a subproject via `FetchContent`:

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
