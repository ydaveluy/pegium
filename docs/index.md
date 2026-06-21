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
    <a class="md-button" href="learn/walkthrough/">Build a language</a>
  </div>
  <div class="pegium-pills">
    <span>C++20</span>
    <span>PEG parser DSL</span>
    <span>LSP</span>
  </div>
</div>

## Why Pegium?

<div class="grid cards" markdown>

-   __Semantics first__

    Shape your language's semantic model directly through C++ AST types and
    grammar assignments, with CST data still available for source-aware tooling.

-   __Lean by default__

    Start from `makeDefaultServices(...)`, keep the default LSP and workspace
    behavior, and override only the language-specific parts.

-   __Parser to editor__

    Use one document model for parsing, linking, diagnostics, formatting,
    completion, rename, references, and other editor features.

-   __Examples that scale__

    Use `arithmetics`, `domainmodel`, `requirements`, and `statemachine` as
    real starting points, not isolated API snippets.

</div>

## Start here

<div class="grid cards" markdown>

-   [Introduction](introduction/index.md)

    Get the high-level picture, then branch into features, showcases, or
    deeper concepts. Want to jump straight in? Build a language end-to-end in
    the [walkthrough](learn/walkthrough.md).

-   [Learn](learn/index.md)

    Follow the recommended path from repository build to grammar, AST,
    references, validation, formatting, and editor services.

-   [Recipes](recipes/index.md)

    Focused guides for common customization tasks: scoping, validation,
    caching, multiple languages, and LSP overrides.

-   [Reference](reference/index.md)

    Canonical subsystem pages for grammar, semantic model, services,
    workspace, and document lifecycle concepts.

-   [Examples](examples/index.md)

    Browse complete example languages and pick the best starting point for
    your project.

</div>

## Related pages

Documentation and examples live in the main repository, so the code and docs stay close together.

- [Repository root](https://github.com/ydaveluy/pegium)
- [Documentation sources](https://github.com/ydaveluy/pegium/tree/main/docs)
- [Examples source tree](https://github.com/ydaveluy/pegium/tree/main/examples)
