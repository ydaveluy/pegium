# Custom Validator

Create a validator class when your language needs semantic rules beyond parsing.

Use this page for the basic validator structure. If your rule needs a
whole-document graph walk, continue with
[Dependency Loops](validation/dependency-loops.md).

## Pattern

1. implement one method per AST node kind you want to validate
2. register the methods in the validation registry
3. report diagnostics with `ValidationAcceptor`

This usually ends up as a small class plus one registry setup point.

## Typical workflow

Start with checks that are obviously local and cheap:

- duplicate names
- empty or missing required values
- inconsistent flags or modifiers

Then add more semantic or cross-reference aware checks once scoping and linking
already work.

## Good practices

- keep checks small and type-specific
- use categories when some checks are expensive
- attach diagnostics to the most precise AST node or property available
- do not duplicate grammar constraints that are already enforced during parsing

## Example direction

Look at the validators in the shipped examples, especially `arithmetics` and
`statemachine`, for the expected structure.

## Related pages

- [Validation](validation/index.md)
- [Dependency Loops](validation/dependency-loops.md)
