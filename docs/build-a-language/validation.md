# Validation

Validation is driven by `pegium::validation::ValidationRegistry`.

Validation is where you express semantic rules that go beyond syntax.

## Model

- register checks by AST node type
- group checks by category such as `fast` and `slow`
- report diagnostics through `ValidationAcceptor`
- run document-level preparation when several checks need the same derived data

Typical examples are:

- uniqueness constraints
- incompatible references
- illegal combinations of modifiers
- domain-specific invariants

## Typical registration pattern

```cpp
registerCheck<MyNode>([](const MyNode &node,
                         const pegium::validation::ValidationAcceptor &acceptor) {
  if (node.name.empty()) {
    acceptor.error(node, "Name must not be empty.");
  }
});
```

This style is useful for small validators or highly local checks.

## Method-based checks

Pegium also supports method-pointer based registration, which keeps validator
implementations close to the owning class and consistent with the formatter API
style.

That style is usually better once the validator grows beyond a few checks:

```cpp
const MyValidator validator;
registry.registerChecks(
    {pegium::validation::ValidationRegistry::makeValidationCheck<
         &MyValidator::checkNode>(validator)});
```

## What to validate

Good validation rules usually depend on already-linked and already-scoped AST:

- does a referenced target exist?
- is a name duplicated in the same scope?
- are the argument counts and relationships acceptable?
- is a state-machine transition legal?

Keep syntactic structure in the grammar. Keep semantic constraints in the
validator.

## Categories

Use categories when some checks are expensive or optional.

The built-in category names include `fast`, `slow`, and `built-in`.

## Practical advice

- attach diagnostics to the smallest useful AST node or property
- keep individual checks focused on one rule
- use document-level preparation if several checks need shared precomputed data
- start with fast checks first and add heavy semantic analysis later
