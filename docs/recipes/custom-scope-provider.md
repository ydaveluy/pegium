# Custom Scope Provider

Customize scoping when visibility rules are more complex than lexical nesting.

If your language only needs package-style or namespace-style qualified names,
start with [Qualified Names](scoping/qualified-names.md). Reach for a custom
scope provider only when lookup behavior itself must change.

## Main entry points

- custom name provider
- custom scope computation
- custom scope provider

Each one solves a different problem:

- name provider: how declarations are named
- scope computation: which symbols are exported and indexed
- scope provider: which symbols are visible at a given reference site

## Recommended order

1. keep the default linker
2. customize exported names if needed
3. customize scope computation for visibility
4. customize the scope provider only when lookup itself needs a special rule

## Typical reasons

- imports
- namespaces
- qualified names
- visibility modifiers
- multi-file symbol aggregation

## Practical advice

Most scoping bugs are easier to diagnose if you first verify exported symbols,
then visible symbols, then final linker behavior. Try not to change all three
layers at once.

## Related pages

- [Scoping](scoping/index.md)
- [Qualified Names](scoping/qualified-names.md)
