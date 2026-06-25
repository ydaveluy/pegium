#pragma once

#include <memory>

#include <arithmetics/core/ArithmeticParser.hpp>
#include <arithmetics/core/Services.hpp>
#include <arithmetics/core/validation/ArithmeticsValidator.hpp>

namespace arithmetics::detail {

/// Applies the arithmetics core overrides to any container that derives from
/// both `pegium::CoreServices` and `ArithmeticsAddedServices` (headless
/// `ArithmeticsCoreServices` or LSP `lsp::ArithmeticsServices`).
///
/// Defined once here so the `core/` and `lsp/` translation units share it.
template <typename Services>
void applyArithmeticsCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::ArithmeticParser>(services);
  services.languageMetaData.fileExtensions = {".calc"};
  services.validator = std::make_unique<validation::ArithmeticsValidator>();
  validation::registerValidationChecks(services);
}

} // namespace arithmetics::detail
