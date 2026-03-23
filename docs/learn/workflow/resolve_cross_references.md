# 5. Resolve Cross-References

Once your language contains names that point to declarations, the syntax tree is
no longer enough on its own. The parser can record the written text of a
reference, but it cannot decide yet what that text means.

That is the purpose of the reference pipeline.

## The problem

Consider this AST shape:

```cpp
struct Feature : pegium::AstNode {
  string name;
  reference<Type> type;
};
```

The parser can fill `type` with the written name, for example `User` or
`blog.User`. But at parse time, that is still only reference text. The real
target node has to be found later.

## The main services

Pegium separates this into several steps:

- `NameProvider` decides how nodes get names
- `ScopeComputation` decides what symbols are exported or cached locally
- `ScopeProvider` decides what is visible from a given reference site
- `Linker` resolves the written name to one concrete target

This separation matters because the same scope information is reused by several
features, especially linking and completion.

For naming, Pegium keeps two related questions separate:

- `getName(...)`: what symbolic name should this node export?
- `getNameNode(...)`: which CST node marks the declaration in source?

That distinction matters when a language normalizes names for indexing but
still wants navigation and rename to target the original declaration text. A
good default naming pattern is described in
[References and Scoping](../../build-a-language/references-and-scoping.md#recommended-naming-pattern).

When you implement editor features, the helper utilities
`named_node_info(...)` and `required_declaration_site_node(...)` let you reuse
that naming contract without rechecking the same CST fallbacks in every
provider.

## Cross-reference resolution from a high-level perspective

1. The parser builds the AST and records reference text.
2. Name and scope computation export symbols into the workspace index and
   prepare document-local scope data.
3. The scope provider exposes the visible candidates for each reference site.
4. The linker resolves the written name to one concrete target.

## A real example

The `domainmodel` example overrides scope computation to export qualified names
for nested types:

```cpp
services->references.scopeComputation =
    std::make_unique<references::DomainModelScopeComputation>(
        *services, qualifiedNameProvider);
```

That customization makes names such as `blog.User` visible in the model while
still letting the default linking flow do most of the heavy lifting. The same
example also uses the recommended AST-backed naming pattern from the reference
guide.

## How to think about the problem

Two ideas are easy to mix up:

- exported symbols: what a document contributes to the workspace index
- visible symbols: what a concrete reference may see at one location

If linking behaves strangely, it is usually best to debug in this order:

1. is the target declaration exported with the right name?
2. does the scope at the reference site contain the right candidates?
3. only then ask whether the linker itself needs to change

## Why this step comes before heavy validation

Many useful features become much easier once references are linked correctly:

- unresolved-name diagnostics
- go to definition
- rename
- workspace-level navigation
- semantic checks that depend on target declarations

## What to expect at the end of this step

At the end of this step, names in your model should resolve to the right target
nodes within one file or across documents, and completion should already be
able to benefit from the same scope information.

## Continue with

- [Qualified Names](../../recipes/scoping/qualified-names.md)
- [Custom Scope Provider](../../recipes/custom-scope-provider.md)
- [Workspace Lifecycle](../../build-a-language/workspace.md)
- [6. Create Validations](create_validations.md)
