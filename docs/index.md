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
    <a class="md-button md-button--primary" href="learn/">Learn Pegium</a>
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

    New here? Start with [Choose Your Path](introduction/choose-your-path.md).

-   [Learn](learn/index.md)

    Follow the recommended Pegium workflow from repository build to grammar,
    AST, references, validation, and editor features.

-   [Recipes](recipes/index.md)

    Jump directly to focused customization tasks such as validators,
    formatters, scoping, completion, hover, and rename.

-   [Reference](reference/index.md)

    Use the API-oriented pages as the canonical reference for grammar,
    semantic model, services, and document lifecycle concepts.

    If you want help choosing the right page, open
    [Start Here](reference/start-here.md).

-   [Examples](examples/index.md)

    See what each shipped example demonstrates and which one to start from.

</div>

## Repository first

Pegium keeps the documentation sources inside the repository so they stay easy
to browse and review on GitHub:

- [Repository root](https://github.com/ydaveluy/pegium)
- [Documentation sources](https://github.com/ydaveluy/pegium/tree/main/docs)
- [Examples source tree](https://github.com/ydaveluy/pegium/tree/main/examples)
