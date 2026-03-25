# Custom Validator

Create a validator class when your language needs semantic rules beyond parsing.

This page covers the basic validator structure. If your rule needs a
whole-document graph walk, continue with
[Dependency Loops](validation/dependency-loops.md).

## Pattern

1. implement one method per AST node kind you want to validate
2. register the methods in the validation registry
3. report diagnostics with `ValidationAcceptor`

This usually ends up as a small class plus one registry setup point.

## A concrete example

The `domainmodel` example uses the following registration pattern:

```cpp
void registerValidationChecks(
    domainmodel::DomainModelServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.domainModel.validation.domainModelValidator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkEntityNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkDataTypeNameStartsWithCapital>(validator)});
}
```

One validator method then attaches a diagnostic directly to the relevant
property:

```cpp
void DomainModelValidator::checkEntityNameStartsWithCapital(
    const Entity &entity,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (entity.name.empty()) {
    return;
  }
  const auto first = entity.name.front();
  if (std::toupper(static_cast<unsigned char>(first)) == first) {
    return;
  }
  accept.warning(entity, "Type name should start with a capital.")
      .property<&Entity::name>();
}
```

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
