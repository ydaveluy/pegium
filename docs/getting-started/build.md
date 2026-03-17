# Build the Repository

## Prerequisites

- a C++20-capable compiler
- CMake
- Ninja or another generator supported by your local setup
- Node.js only if you want to run the VS Code clients shipped in the examples

## Repository layout at a glance

After cloning the repository, the main directories to know are:

- `src/pegium/` for the framework
- `examples/` for end-to-end languages
- `tests/` for framework tests
- `docs/` for this documentation

The build output goes under `build/` by default.

## Configure and build

=== "Configure"

    ```bash
    cmake -S . -B build
    ```

=== "Build"

    ```bash
    cmake --build build -j
    ```

=== "Run tests"

    ```bash
    cd build
    ctest --output-on-failure
    ```

## Build the documentation locally

```bash
python3 -m pip install -r requirements-docs.txt
mkdocs serve
```

Use `mkdocs build --strict` to validate links and navigation before pushing
documentation changes.

## What to do after the build

Once the repository builds successfully:

- run one of the example CLIs to see a complete parser in action
- run one of the example LSP binaries if you want the editor path
- pick a source example before creating a new language from scratch
