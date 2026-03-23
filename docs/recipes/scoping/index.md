# Scoping

You already know scopes from programming: some names are visible only in
certain parts of a program, while others are exported for wider lookup.

The same happens in Pegium. Once your language has names and references,
scoping determines what can be seen from a given reference site and what stays
hidden.

In Pegium, scoping customization usually lives in one of three layers:

- naming, which decides how declarations are exported
- scope computation, which decides which symbols exist locally or globally
- scope lookup, which decides which of those symbols are visible at a reference
  site

In terms of the document lifecycle, that breaks down into three phases:

- symbol indexing for globally exported names
- scope computation for local symbol availability
- linking for final reference resolution

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
- [Document Lifecycle](../../reference/document-lifecycle.md)
- [LSP Services](../../build-a-language/lsp-services.md)
