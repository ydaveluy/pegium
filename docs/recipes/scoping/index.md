# Scoping

Scoping decides what a reference can see. Just like in a programming language, some names are visible only locally while others are exported for wider lookup.

Once your language has names and references, scoping determines what is visible from a given reference site and what stays hidden. You customize it in one of three layers:

- **Naming** — how declarations are exported.
- **Scope computation** — which symbols exist locally or globally.
- **Scope lookup** — which of those symbols are visible at a reference site.

These map to three phases of the document lifecycle:

- **Symbol indexing** — globally exported names.
- **Scope computation** — local symbol availability.
- **Linking** — final reference resolution.

## Where to start

Pick the narrowest change that solves your problem:

- Use [Qualified Names](qualified-names.md) when declarations should be exported as `package.Type` or `namespace.symbol`.
- Use [Custom Scope Provider](../custom-scope-provider.md) when lookup needs different visibility rules.

## Practical advice

Debug scoping problems in this order:

1. Verify how declarations are named.
2. Verify which symbols are exported or kept local.
3. Verify lookup behavior at the reference site.

## Related pages

- [Semantic Model](../../reference/semantic-model.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
- [LSP Services](../../build-a-language/lsp-services.md)
