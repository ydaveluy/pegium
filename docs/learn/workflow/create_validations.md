# 6. Create Validations

After resolving references, you can assume that the model is structurally
usable. Now you can start validating whether it also makes sense for your
domain.

This is where semantic rules live.

## Syntax versus validation

The grammar answers questions like:

- does this file follow the syntax?
- are the required tokens present?
- can this construct be parsed?

Validation answers different questions:

- is this name duplicated?
- is this state transition legal?
- are these references compatible?
- does this model violate a domain rule even though it parses correctly?

## Example

The `statemachine` example registers validator methods like this:

```cpp
const StatemachineValidator validator;
registry.registerChecks(
    {pegium::validation::ValidationRegistry::makeValidationCheck<
         &StatemachineValidator::checkStateNameStartsWithCapital>(validator),
     pegium::validation::ValidationRegistry::makeValidationCheck<
         &StatemachineValidator::checkUniqueNames>(validator)});
```

One of the checks then emits a property-focused warning:

```cpp
void StatemachineValidator::checkStateNameStartsWithCapital(
    const State &state,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (state.name.empty()) {
    return;
  }
  const auto first = state.name.front();
  if (std::toupper(static_cast<unsigned char>(first)) != first) {
    accept.warning(state, "State name should start with a capital letter.")
        .property<&State::name>();
  }
}
```

This is a good illustration of the Pegium style:

- checks are typed
- registration is explicit
- diagnostics can be attached to a whole node or a specific property
- validator registration usually happens during bootstrap, while still allowing
  rare additions between validation passes

## Recommended first validations

Start with cheap and obviously useful checks:

- duplicate names
- missing or suspicious values
- incompatible local combinations
- unresolved or impossible references

Then add heavier, more global checks only once the earlier parts of the
language are stable.

## What to expect at the end of this step

At the end of this step, invalid-but-parseable models should produce clear
diagnostics attached to useful nodes or properties.

## Continue with

- [Validation](../../recipes/validation/index.md)
- [Custom Validator](../../recipes/custom-validator.md)
- [7. Add Formatting and LSP Services](generate_everything.md)
