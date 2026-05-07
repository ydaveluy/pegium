# Pegium's Workflow

Pegium's workflow can be summarized as a short sequence of practical steps:

1. build the repository
2. choose a starting point
3. write the grammar
4. shape the AST and CST
5. resolve cross-references
6. create validations
7. add formatting and LSP services

The exact needs of a real project may go beyond this sequence, but this is the
common path we recommend for building a language with Pegium. For advanced
topics and one-off customization tasks, continue with the
[recipes](../../recipes/index.md).

## Explanation

This workflow breaks down into three parts:

- the initial setup, which you normally do once
- the core language work, which you revisit as the grammar evolves
- advanced customization, which is usually better handled through the
  [recipes](../../recipes/index.md)

## Initial setup

### [1. Build the Repository](install.md)

Optional: clone Pegium and build it once to confirm the framework runs
on your machine. Skip this step entirely if you only want to build your
own language — Pegium will be pulled in as a `FetchContent` dependency.

### [2. Choose a Starting Point](scaffold.md)

Start from the
[`pegium-language-template`](https://github.com/ydaveluy/pegium-language-template)
GitHub template repository. Pegium is pulled in as a `FetchContent`
dependency; the bundled `cmake -P` script renames every identifier to
your own in one command.

## Core workflow

### [3. Write the Grammar](write_grammar.md)

Define the surface syntax with `PegiumParser`, terminals, rules, skippers, and
parser expressions.

### [4. Shape the AST and CST](generate_ast.md)

Use assignments, actions, and CST-backed parsing to build the semantic and
source-aware structure of your language.

### [5. Resolve Cross-References](resolve_cross_references.md)

Export names, compute scopes, and link references so that navigation and
workspace features have real semantic targets.

### [6. Create Validations](create_validations.md)

Add semantic rules once the AST and references are stable enough to reason
about the model.

### [7. Add Formatting and LSP Services](generate_everything.md)

Finish the user-facing tooling by wiring formatter, hover, completion, rename,
and workspace behavior where needed.

## Advanced topics

Everything beyond the common workflow is better handled through the
[recipes](../../recipes/index.md) and [reference](../../reference/index.md)
pages.
