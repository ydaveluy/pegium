---
title: Pegium
template: home.html
---

<div class="pegium-hero">
  <p class="pegium-eyebrow">C++20 language engineering toolkit</p>
  <h1>Bring language engineering to modern C++.</h1>
  <p class="pegium-lead">
    Pegium is an open source toolkit for building textual languages with
    first-class support for parsing, AST/CST construction, references,
    validation, formatting, and language-server features.
  </p>
  <div class="pegium-actions">
    <a class="md-button md-button--primary" href="introduction/">Read the introduction</a>
    <a class="md-button" href="learn/workflow/">Start the workflow</a>
  </div>
  <div class="pegium-pills">
    <span>C++20</span>
    <span>PEG parser DSL</span>
    <span>LSP</span>
  </div>
</div>

## Why Pegium?

<div class="grid cards" markdown>

-   __Semantics First__

    Shape the semantic model of your language directly through C++ AST types
    and grammar assignments, while still keeping CST data available for
    source-aware tooling.

-   __Lean By Default__

    Start from `makeDefaultServices(...)`, keep the default LSP and workspace
    behavior where it helps, and override only the parts that are
    language-specific.

-   __Parser To Editor__

    Use one coherent document model for parsing, linking, diagnostics,
    formatting, completion, rename, references, and other editor-facing
    features.

-   __Examples That Scale__

    Explore `arithmetics`, `domainmodel`, `requirements`, and `statemachine`
    as real starting points instead of isolated API snippets.

</div>

## Start Here

<div class="grid cards" markdown>

-   [Introduction](introduction/index.md)

    Start with the high-level picture, then branch into features, showcases,
    or deeper concepts.

    If you want a guided orientation page first, open
    [Choose Your Path](introduction/choose-your-path.md).

-   [Learn](learn/index.md)

    Follow the recommended path from repository build to grammar, AST,
    references, validation, formatting, and editor services.

-   [Recipes](recipes/index.md)

    Use focused guides for common customization tasks such as scoping,
    validation, caching, multiple languages, and LSP overrides.

-   [Reference](reference/index.md)

    Use the canonical subsystem pages for grammar, semantic model, services,
    workspace, and document lifecycle concepts.

-   [Examples](examples/index.md)

    Inspect complete example languages and choose the best starting point for
    your own project.

</div>

## Repository

Pegium keeps its documentation and examples in the main repository so the code
and the docs stay close to each other:

- [Repository root](https://github.com/ydaveluy/pegium)
- [Documentation sources](https://github.com/ydaveluy/pegium/tree/main/docs)
- [Examples source tree](https://github.com/ydaveluy/pegium/tree/main/examples)
