# Validation

Validation is driven by `pegium::validation::ValidationRegistry`.

## Model

- register checks by AST node type
- group checks by category such as `fast` and `slow`
- report diagnostics through `ValidationAcceptor`
- run document-level preparation before or after per-node checks when needed
- usually register checks during service bootstrap, while keeping the option to
  add rare late registrations between validation passes

Validation is where you express semantic rules that go beyond syntax:

- uniqueness constraints
- unresolved or incompatible references
- illegal combinations of modifiers or declarations
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

This registration style is useful for small validators or highly local checks.

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

Use categories when some checks are expensive or optional. The default document
validator can then run subsets depending on the current context.

The built-in category names include:

- `fast`
- `slow`
- `built-in`

Use categories when some checks traverse a lot of the model or require
cross-document work.

At runtime, Pegium prepares an immutable snapshot of the selected categories
once per validation pass, then reuses that prepared set for all visited nodes.
Late registrations invalidate future snapshots, without changing a pass that is
already running.

## Practical advice

- attach diagnostics to the smallest useful AST node or property
- keep individual checks focused on one rule
- use document-level preparation if several checks need shared precomputed data
- start with fast checks first and add heavy semantic analysis later
