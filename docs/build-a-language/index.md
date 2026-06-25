# Build a Language

Once a project exists, the usual order is: **define your AST types**, **declare the grammar** that fills them, then **customize the services** (scoping, validation, formatting, editor features).

Reach for these guides once you know the overall workflow and want to dig into a single problem.

## Topics

- [Define the AST](ast-and-cst.md) — your semantic model: `AstNode`, `NamedAstNode`, and the field types.
- [Grammar essentials](grammar.md) — terminals, rules, and every combinator.
- [References and scoping](references-and-scoping.md)
- [Validation](validation.md)
- [Formatting](formatting.md)
- [LSP services](lsp-services.md)
- [Workspace lifecycle](workspace.md)

## Related pages

- [Learn](../learn/index.md) — the recommended order of implementation.
- [Reference](../reference/index.md) — the exact user-facing concepts and APIs behind each subsystem.
