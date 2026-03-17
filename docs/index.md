---
title: Pegium
template: home.html
---

<div class="pegium-hero">
  <p class="pegium-eyebrow">C++20 language engineering toolkit</p>
  <h1>Build parsers, AST/CST pipelines, validators, references, formatters, and language servers.</h1>
  <p class="pegium-lead">
    Pegium packages the core building blocks required to implement textual
    languages in modern C++, from grammar definitions to LSP features.
  </p>
  <div class="pegium-actions">
    <a class="md-button md-button--primary" href="getting-started/">Get started</a>
    <a class="md-button" href="examples/">Browse examples</a>
  </div>
  <div class="pegium-pills">
    <span>C++20</span>
    <span>PEG parser DSL</span>
    <span>AST + CST</span>
    <span>Validation</span>
    <span>Formatting</span>
    <span>LSP</span>
  </div>
</div>

## What you get

<div class="grid cards" markdown>

-   __Parser DSL__

    Build grammars with C++ expressions, named rules, typed terminals,
    parser rules, actions, assignments, repetitions, and skippers.

-   __Syntax trees__

    Work with `AstNode`, `CstNodeView`, references, and utility helpers for
    traversal, selections, and lookups.

-   __Language services__

    Compose validation, scoping, linking, formatting, and default LSP
    providers through the Pegium services model.

-   __Examples__

    Explore `arithmetics`, `domainmodel`, `requirements`, and `statemachine`
    for end-to-end reference implementations.

</div>

## Documentation map

<div class="grid cards" markdown>

-   [Introduction](introduction/index.md)

    Understand where Pegium fits, what problems it solves, and how it relates
    to Langium.

-   [Getting Started](getting-started/index.md)

    Build the repository, run the examples, and follow a first language path.

-   [Build a Language](build-a-language/index.md)

    Learn the main extension points: grammar, AST/CST, references, validation,
    formatting, LSP services, and workspace lifecycle.

-   [Reference](reference/index.md)

    Use the API-oriented pages as the canonical reference for grammar,
    formatter DSL, services, and workspace concepts.

-   [Recipes](recipes/index.md)

    Jump directly to focused customization tasks such as validators,
    formatters, scoping, completion, hover, and rename.

-   [Examples](examples/index.md)

    See what each shipped example demonstrates and which one to start from.

</div>

## Repository first

Pegium keeps the documentation sources inside the repository so they stay easy
to browse and review on GitHub:

- [Repository root](https://github.com/ydaveluy/pegium)
- [Documentation sources](https://github.com/ydaveluy/pegium/tree/main/docs)
- [Examples source tree](https://github.com/ydaveluy/pegium/tree/main/examples)
