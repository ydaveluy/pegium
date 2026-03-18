# Pegium's Workflow

Pegium's workflow can be expressed as a short sequence of practical steps:

1. build the repository
2. choose a starting point
3. write the grammar
4. shape the AST and CST
5. resolve cross-references
6. create validations
7. add formatting and LSP services

The exact needs of a real project may go beyond this sequence, but this is the
recommended common path for building a language with Pegium.

## Start here if

- you want the recommended order of implementation
- you are turning one of the examples into your own language
- you need to know which tasks are foundational and which ones come later

## Explanation

You can think of the workflow as three blocks:

- environment and repository setup
- the core language loop you revisit as the grammar evolves
- advanced customization, which is usually better handled through the
  [recipes](../../recipes/index.md)

## Initial setup

### [1. Build the Repository](install.md)

Build Pegium itself, confirm the examples compile, and make sure the test suite
is available locally.

### [2. Choose a Starting Point](scaffold.md)

Pick the example whose structure is closest to your future language instead of
starting from an empty directory.

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

## Rule of thumb

Do not over-customize early. Get parsing, AST shape, references, and
validation working first, then move on to formatter and LSP refinements.
