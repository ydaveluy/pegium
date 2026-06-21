# Validation

Validation starts where parsing stops. Once the grammar guarantees the text is structurally valid, you use validation to express the language's semantic rules:

- duplicate names
- wrong argument counts
- forbidden recursion
- inconsistent state transitions
- project-specific conventions

By the time validation runs, Pegium has parsed the document and, in the usual workflow, linked its references. That makes validation the right place for rules that need semantic context rather than raw syntax.

## Where to start

Pick the narrowest rule that has the right view of the model:

- node-level checks for local constraints — one typed check per AST node kind
- model-level checks for whole-document properties such as duplicates or cycles — one root-model check when the rule needs a graph or an aggregate view

Start with [Dependency Loops](dependency-loops.md), which attaches a whole-document rule to a real Pegium example. For the general structure of a validator class and registry setup, see [Custom Validator](../custom-validator.md).

## Related pages

- [Custom Validator](../custom-validator.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
- [Configuration Services](../../reference/configuration-services.md)
