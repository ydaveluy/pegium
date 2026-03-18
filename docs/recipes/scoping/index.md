# Scoping

Use the scoping recipes when name visibility is no longer a simple matter of
"look in the current container, then in the workspace".

In Pegium, scoping customization usually lives in one of three layers:

- naming, which decides how declarations are exported
- scope computation, which decides which symbols exist locally or globally
- scope lookup, which decides which of those symbols are visible at a reference
  site

## Where to start

Start with the narrowest change that solves your problem:

- Use [Qualified Names](qualified-names.md) when declarations should be exported
  as `package.Type` or `namespace.symbol`.
- Use [Custom Scope Provider](../custom-scope-provider.md) when lookup itself
  needs different visibility rules.

Most scoping problems are easier to debug in this order:

1. verify how declarations are named
2. verify which symbols are exported or kept local
3. verify lookup behavior at the reference site

## Related pages

- [Semantic Model](../../reference/semantic-model.md)
- [Workspace Concepts](../../reference/workspace.md)
- [Default LSP Services](../../reference/lsp-services.md)
