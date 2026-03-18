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

## A living example

In `examples/arithmetics/src/validation/ArithmeticsValidator.cpp`, recursion is
checked in `checkModule(...)`.

The validator first registers a model-level check:

```cpp
registry.registerChecks(
    {pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkDivisionByZero>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkDefinition>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkFunctionCall>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &ArithmeticsValidator::checkModule>(validator)});
```

`checkModule(...)` then gets the whole `Module`, which is the right level to
see all definitions and calls together.

## Step 1: build a dependency graph from the AST

The example walks each definition, collects nested `FunctionCall` nodes, and
records edges by name:

```cpp
for (const auto &statement : module.statements) {
  const auto *definition = dynamic_cast<const Definition *>(statement.get());
  if (definition == nullptr) {
    continue;
  }

  std::vector<const FunctionCall *> calls;
  collect_function_calls(definition->expr.get(), calls);
  for (const auto *call : calls) {
    const auto *callee = as_definition(call->func.get());
    if (callee == nullptr) {
      continue;
    }
    graph[definition->name].push_back(callee->name);
  }
}
```

This keeps the validation logic independent from LSP concerns. By the time
validation runs, references have already been linked, so `call->func.get()`
can resolve directly to the target definition.

## Step 2: detect the cycle

The same validator uses a depth-first search with three states:

- `Unvisited`
- `Visiting`
- `Visited`

Seeing a node already marked as `Visiting` means the walk just found a cycle.
That is enough for many language rules, and it avoids bringing in an extra
graph dependency when you only need cycle detection.

## Step 3: report the most precise diagnostic you can

Once a cycle is found, the example reports the error on the definition name
when possible:

```cpp
if (hasCycle) {
  if (cycleDefinition != nullptr) {
    accept.error(*cycleDefinition, "Recursion is not allowed.")
        .property<&Definition::name>();
  } else {
    accept.error(module, "Recursion is not allowed.");
  }
}
```

That pattern is worth copying: compute globally, but attach the diagnostic to
the smallest useful node.

## Simple cycles versus complex graphs

Not every cycle check needs a full graph walk.

For simple `1:n` relations such as a parent chain, walking upward while keeping
track of visited nodes is often enough.

For call graphs, import graphs, or other `n:m` relations, a graph-oriented
approach like the `arithmetics` example scales better and is usually easier to
extend.

## Related pages

- [Examples: Arithmetics](../../examples/arithmetics.md)
- [Custom Validator](../custom-validator.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
