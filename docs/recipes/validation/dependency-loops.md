# Dependency Loops

Detect recursion or dependency cycles during validation. Pegium's `arithmetics` example does exactly this: function definitions may call each other, but recursive cycles are not allowed.

## What is the problem?

Cycle detection rarely depends on one node in isolation. You inspect a larger graph:

- function calls
- imports between files
- type dependencies
- state transitions

So this check usually belongs on the root model node, not on the individual reference node.

## Two common cases

### Simple parent-style relations

If the dependency shape is `1:n`, detect cycles by walking one chain and remembering the visited nodes. Parent links and superclass chains fit here.

### General graphs

If the shape is `n:m`, a graph-oriented approach is clearer. Function calls, imports, and most dependency networks fall here.

That is the case in `arithmetics`: one definition may call several others and be called from many places.

## A living example

In `examples/arithmetics/src/arithmetics/core/validation/ArithmeticsValidator.cpp`, recursion is checked in `checkFunctionRecursion(...)`.

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

`checkFunctionRecursion(...)` then receives the whole `Module`, the right level to see all definitions and calls together.

## Step 1: build the dependency view from the AST

The example walks each definition, collects nested `FunctionCall` nodes, and follows resolved targets:

```cpp
for (const auto &statement : module.statements) {
  const auto *definition = pegium::ast_ptr_cast<const Definition>(statement);
  if (definition == nullptr) {
    continue;
  }

  auto remainingCalls =
      get_not_traversed_nested_calls(*definition, traversedFunctions);
  // ...
}
```

Linking runs before validation, so the validator works from resolved target definitions instead of raw reference text.

## Step 2: detect the cycle

The validator walks those call relationships and records the recursive paths. A visited-set walk, a DFS with visitation states, or a graph library all work, depending on your problem's shape.

The key point: attach the validation to the whole model, not to one isolated reference.

## Step 3: report the most precise diagnostic you can

Once a cycle is found, the example reports the error on the individual call sites participating in that cycle:

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

Compute globally, but attach the diagnostic to the smallest useful node or property.

## Loop detection versus dependency ordering

Some domains need more than "is there a cycle?".

If the graph is acyclic, you may also want a dependency order for later processing, code generation, or evaluation. The same graph view used for cycle detection is often the right basis for that second step.

## When to use this pattern

This pattern fits when:

- the rule depends on relationships between many nodes
- one node can depend on several others
- a local validator method would not have enough context
- the best diagnostic still needs to point back to one concrete node

## Related pages

- [Examples: Arithmetics](../../examples/arithmetics.md)
- [Custom Validator](../custom-validator.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
