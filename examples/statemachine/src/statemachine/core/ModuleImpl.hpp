#pragma once

#include <memory>

#include <statemachine/core/Services.hpp>
#include <statemachine/core/validation/StatemachineValidator.hpp>

namespace statemachine::parser {
std::unique_ptr<const pegium::parser::Parser>
makeStateMachineParser(const pegium::CoreServices &services);
} // namespace statemachine::parser

namespace statemachine::detail {

/// Applies the statemachine core overrides to any container that derives from
/// both `pegium::CoreServices` and `StatemachineAddedServices` (headless
/// `StatemachineCoreServices` or LSP `lsp::StatemachineServices`).
///
/// Defined once here so the `core/` and `lsp/` translation units share it.
template <typename Services>
void applyStatemachineCoreModule(Services &services) {
  services.parser = parser::makeStateMachineParser(services);
  services.languageMetaData.fileExtensions = {".statemachine"};
  services.validator = std::make_unique<validation::StatemachineValidator>();
  validation::registerValidationChecks(services);
}

} // namespace statemachine::detail
