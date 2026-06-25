#pragma once

#include <memory>

#include <requirements/core/Parser.hpp>
#include <requirements/core/Services.hpp>
#include <requirements/core/validation/RequirementsValidator.hpp>
#include <requirements/core/validation/TestsValidator.hpp>

namespace requirements::detail {

/// Applies the requirements-language core overrides to any container that
/// derives from both `pegium::CoreServices` and `RequirementsAddedServices`
/// (headless `RequirementsCoreServices` or LSP `lsp::RequirementsServices`).
///
/// Defined once here so the `core/` and `lsp/` translation units share it.
template <typename Services>
void applyRequirementsCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::RequirementsParser>(services);
  services.languageMetaData.fileExtensions = {".req"};
  services.validator =
      std::make_unique<validation::RequirementsValidator>(services.shared);
  validation::registerRequirementsValidationChecks(services);
}

/// Applies the tests-language core overrides; see `applyRequirementsCoreModule`.
template <typename Services>
void applyTestsCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::TestsParser>(services);
  services.languageMetaData.fileExtensions = {".tst"};
  services.validator = std::make_unique<validation::TestsValidator>();
  validation::registerTestsValidationChecks(services);
}

} // namespace requirements::detail
