# Validation

Validation starts where parsing stops. Once the grammar guarantees that the
text is structurally valid, validation expresses the semantic rules of the
language.

Validation is where you express semantic rules such as:

- duplicate names
- wrong argument counts
- forbidden recursion
- inconsistent state transitions
- project-specific conventions

By the time validation runs, Pegium has already parsed the document and, in the
usual workflow, linked references as well. That is why validation is the right
place for rules that need semantic context rather than raw syntax.

## Where to start

Pick the narrowest rule that has the right view of the model:

- node-level checks for local constraints
- model-level checks for whole-document properties such as duplicates or cycles

In practice, that often means:

- one typed check for one AST node kind when the rule is local
- one root-model check when the rule needs to see a graph or an aggregate view
  of the document

The best next page here is [Dependency Loops](dependency-loops.md), which shows
how to attach a whole-document rule to a real Pegium example.

If you only need the general structure of a validator class and registry setup,
see [Custom Validator](../custom-validator.md).

## Related pages

- [Custom Validator](../custom-validator.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
- [Configuration Services](../../reference/configuration-services.md)
