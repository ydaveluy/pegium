# Validation

Validation is where you express semantic rules that go beyond syntax. It is driven by `pegium::validation::ValidationRegistry`.

This is the subsystem walkthrough. For recipe-style task pages, see [Custom Validator](../recipes/custom-validator.md) for the class layout and [Dependency Loops](../recipes/validation/dependency-loops.md) for whole-document graph checks.

## Model

- Register checks by AST node type.
- Group checks by category such as `fast` and `slow`.
- Report diagnostics through `ValidationAcceptor`.
- Run document-level preparation when several checks need the same derived data.

Typical examples:

- Uniqueness constraints.
- Incompatible references.
- Illegal combinations of modifiers.
- Domain-specific invariants.

## Typical registration pattern

Register checks on the `ValidationRegistry` you obtain from the service container, typically inside a `registerValidationChecks(services)` function:

```cpp
auto &registry = *services.validation.validationRegistry;

registry.registerCheck<MyNode>(
    [](const MyNode &node,
       const pegium::validation::ValidationAcceptor &acceptor) {
      if (node.name.empty()) {
        acceptor.error(node, "Name must not be empty.");
      }
    });
```

This style suits small validators or highly local checks.

## Method-based checks

Pegium also supports method-pointer based registration. It keeps validator implementations close to the owning class and consistent with the formatter API style.

This style is usually better once the validator grows beyond a few checks:

```cpp
auto &validator = *services.validator;
registry.registerChecks(
    {pegium::validation::ValidationRegistry::makeValidationCheck<
         &MyValidator::checkNode>(validator)});
```

`makeValidationCheck<&Method>` binds to the validator **by reference**: the registry
does not copy or own it, so every check runs on the single instance owned by the
service container (`services.validator`). That instance must outlive the registry
— which it does — and it may safely hold per-build state, since it is never
duplicated.

## What to validate

Good validation rules usually depend on already-linked and already-scoped AST:

- Does a referenced target exist?
- Is a name duplicated in the same scope?
- Are the argument counts and relationships acceptable?
- Is a state-machine transition legal?

Keep syntactic structure in the grammar. Keep semantic constraints in the validator.

## Categories

Use categories when some checks are expensive or optional. The built-in category names include `fast`, `slow`, and `built-in`.

## Practical advice

- Attach diagnostics to the smallest useful AST node or property.
- Keep individual checks focused on one rule.
- Use document-level preparation if several checks need shared precomputed data.
- Start with fast checks first and add heavy semantic analysis later.

## Related pages

- [Custom Validator](../recipes/custom-validator.md)
- [Dependency Loops](../recipes/validation/dependency-loops.md)
