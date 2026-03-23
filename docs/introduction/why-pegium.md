# Why Pegium

Pegium focuses on a simple idea: language tooling should feel like framework
work, not like wiring dozens of unrelated utilities by hand.

It aims to give C++ projects a coherent language workbench experience while
staying honest about the fact that the implementation model is different from
parser generators or external grammar systems.

## Design goals

- Use modern C++20 idioms instead of external DSL files.
- Keep parser, syntax-tree, validation, and LSP pieces in one consistent
  framework.
- Make the common path explicit with shipped default services and examples.
- Expose strong customization points where language-specific behavior matters.

## Typical use cases

- internal DSLs backed by a C++ codebase
- language servers for configuration or modeling languages
- parser-heavy tools that still need AST, linking, validation, or formatting
- teams that want a single toolkit for parser, semantics, and editor services

## What makes it practical

Pegium tries to make the common path short:

- the examples are full end-to-end languages, not isolated API fragments
- `makeDefaultServices(...)` gives you a usable baseline quickly
- formatter, validation, scoping, and LSP customization all follow explicit
  class-based patterns
- the AST and CST stay available, so semantics and source-aware tooling can
  both be implemented cleanly

## Why not just use a parser library

A parser library alone solves only the first step. Real language tooling also
needs:

- typed trees
- symbol visibility and linking
- diagnostics
- formatting
- document lifecycle management
- editor protocol integration

Pegium packages those layers together so they share the same model and runtime
infrastructure.

## What Pegium is not

Pegium is not a website generator, IDE, or code generator. It gives you a
language implementation toolkit and example applications, then lets you decide
how far you want to push the surrounding tooling.

## Continue with

- [Features](features.md) for the concrete capability list
- [Showcases](showcases.md) if you want to inspect complete example languages
- [Learn Pegium](../learn/index.md) if you already want to start building
