# Validation

Use the validation recipes when syntax alone is no longer enough to protect the
user from invalid models.

Validation is where you express semantic rules such as:

- duplicate names
- wrong argument counts
- forbidden recursion
- inconsistent state transitions
- project-specific conventions

## Where to start

Pick the narrowest rule that has the right view of the model:

- node-level checks for local constraints
- model-level checks for whole-document properties such as duplicates or cycles

The best next page here is [Dependency Loops](dependency-loops.md), which shows
how to attach a whole-document rule to a real Pegium example.

If you only need the general structure of a validator class and registry setup,
see [Custom Validator](../custom-validator.md).

## Related pages

- [Custom Validator](../custom-validator.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
- [Configuration Services](../../reference/configuration-services.md)
