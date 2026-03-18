# 1. Build the Repository

Start by building Pegium itself and running at least one example locally.

## Why this comes first

Before designing your own language, you want to know that:

- your compiler and CMake setup work
- the examples build on your machine
- the repository layout makes sense to you

## Recommended steps

1. Configure and build the repository with CMake.
2. Run at least one shipped example locally.
3. Keep the repository structure in mind:
   `src/pegium/` for the framework, `examples/` for working languages,
   `tests/` for usage patterns.

## Outcome

At the end of this step, you should have a working build and at least one
example binary that you can run locally.

## Continue with

- [2. Choose a Starting Point](scaffold.md)
- [Examples Overview](../../examples/index.md)
