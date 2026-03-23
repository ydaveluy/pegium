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
cmake --build build
```

If you also want to validate the test suite:

```bash
cd build
ctest --output-on-failure
```

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
