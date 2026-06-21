# Custom Validator

Add semantic rules beyond parsing by writing a validator class. This page covers the basic structure; for rules that need a whole-document graph walk, see [Dependency Loops](validation/dependency-loops.md).

## Pattern

1. implement one method per AST node kind you want to validate
2. register the methods in the validation registry
3. report diagnostics with `ValidationAcceptor`

This is usually a small class plus one registry setup point.

## A concrete example

The `domainmodel` example registers checks like this:

```cpp
template <typename TServices>
void registerValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkEntityNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkDataTypeNameStartsWithCapital>(validator)});
}
```

The template runs the same helper on either the core or the LSP service container. Both inherit `validation.validationRegistry` and expose the language's validator via `services.validator`.

A validator method then attaches a diagnostic directly to the relevant property:

```cpp
void DomainModelValidator::checkEntityNameStartsWithCapital(
    const ast::Entity &entity,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (entity.name.empty()) {
    return;
  }
  const auto first = entity.name.front();
  if (std::toupper(static_cast<unsigned char>(first)) == first) {
    return;
  }
  accept.warning(entity, "Type name should start with a capital.")
      .property<&ast::Entity::name>();
}
```

## Typical workflow

Start with checks that are local and cheap:

- duplicate names
- empty or missing required values
- inconsistent flags or modifiers

Add semantic or cross-reference aware checks once scoping and linking work.

## Practical advice

- keep checks small and type-specific
- use categories when some checks are expensive
- attach diagnostics to the most precise AST node or property available
- do not duplicate grammar constraints already enforced during parsing

For expected structure, look at the validators in the `arithmetics` and `statemachine` examples.

## Related pages

- [Validation](validation/index.md)
- [Dependency Loops](validation/dependency-loops.md)
