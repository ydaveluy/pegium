# Dependency Loops

This recipe shows how to detect recursion or dependency cycles during
validation.

Pegium's `arithmetics` example already contains a concrete version of this
pattern: function definitions may call each other, but recursive cycles are not
allowed.

## What is the problem?

Cycle detection is rarely a property of one node in isolation. You usually need
to inspect a larger graph:

- function calls
- imports between files
- type dependencies
- state transitions

That is why this kind of check often belongs on the root model node rather than
on the individual reference node.

## Two common cases

### Simple parent-style relations

If the dependency shape is basically `1:n`, you can often detect cycles by
walking one chain while remembering the visited nodes. Typical examples are
parent links or superclass chains.

### General graphs

If the dependency shape is really `n:m`, a graph-oriented approach is usually
clearer. Function calls, imports, and many dependency networks fall into that
category.

That is the case in `arithmetics`, where one definition may call several other
definitions and may itself be called from many places.

## A living example

In `examples/arithmetics/src/core/validation/ArithmeticsValidator.cpp`, recursion is
checked in `checkFunctionRecursion(...)`.

The validator first registers a model-level check:

```cpp
registry.registerChecks(
    {pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkDivByZero>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkUniqueParameters>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkNormalizable>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkUniqueDefinitions>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkFunctionRecursion>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkMatchingParameters>(validator)});
```

`checkFunctionRecursion(...)` then gets the whole `Module`, which is the right
level to see all definitions and calls together.

## Step 1: build the dependency view from the AST

The example walks each definition, collects nested `FunctionCall` nodes, and
follows resolved targets:

```cpp
for (const auto &statement : module.statements) {
  const auto *definition = dynamic_cast<const Definition *>(statement.get());
  if (definition == nullptr) {
    continue;
  }

  auto remainingCalls =
      get_not_traversed_nested_calls(*definition, traversedFunctions);
  // ...
}
```

Because linking already ran before validation, the validator can work from
resolved target definitions instead of raw reference text.

## Step 2: detect the cycle

The same validator then walks those call relationships and records the
recursive paths it finds. Whether you use a visited-set walk, a DFS with
visitation states, or a graph library depends on the shape of your problem.

The important part is that the validation is attached to the whole model, not
to one isolated reference.

## Step 3: report the most precise diagnostic you can

Once a cycle is found, the example reports the error on the individual call
sites participating in that cycle:

```cpp
for (const auto &cycle : callCycles) {
  const auto cycleMessage = print_cycle(cycle, callsTree);
  for (const auto &entry : iterate_back(cycle, callsTree)) {
    accept
        .error(*entry.call,
               std::format("Recursion is not allowed [{}]", cycleMessage))
        .property<&FunctionCall::func>();
  }
}
```

That pattern is worth keeping in mind: compute globally, but attach the
diagnostic to the smallest useful node or property.

## Loop detection versus dependency ordering

Some domains need more than "is there a cycle?".

If the dependency graph is acyclic, you may also want a dependency order for
later processing, code generation, or evaluation. The same graph view that
helps with cycle detection is often the right basis for that second step as
well.

## When to use this pattern

This pattern is a good fit when:

- the rule depends on relationships between many nodes
- one node can depend on several others
- a local validator method would not have enough context
- the best diagnostic still needs to point back to one concrete node

## Related pages

- [Examples: Arithmetics](../../examples/arithmetics.md)
- [Custom Validator](../custom-validator.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
