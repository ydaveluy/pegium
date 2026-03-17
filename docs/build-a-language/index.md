# Build a Language

Pegium follows a clear pipeline:

1. parse text into CST + AST
2. resolve references and compute scopes
3. validate semantic rules
4. provide editor tooling through services and LSP providers

Use this section as the end-to-end guide to that pipeline.

The order of the pages matches a practical implementation sequence. Most
projects go through these stages in roughly this order:

1. grammar
2. AST shape
3. references and linking
4. validation
5. formatting
6. editor and workspace integration

## Topics

- [Grammar essentials](grammar.md)
- [AST and CST](ast-and-cst.md)
- [References and scoping](references-and-scoping.md)
- [Validation](validation.md)
- [Formatting](formatting.md)
- [LSP services](lsp-services.md)
- [Workspace lifecycle](workspace.md)

## Recommended usage

- Read [Grammar essentials](grammar.md) and [AST and CST](ast-and-cst.md)
  together.
- Read [References and scoping](references-and-scoping.md) before validation if
  your language contains names that resolve to declarations.
- Add formatting after the grammar and CST shape have stabilized enough that
  source-level selections will not keep changing every day.
- Read [LSP services](lsp-services.md) and [Workspace lifecycle](workspace.md)
  once the language already parses and links correctly.
