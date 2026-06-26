#pragma once

#include <memory>

#include <requirements/core/Services.hpp>
#include <requirements/core/validation/RequirementsValidator.hpp>
#include <requirements/core/validation/TestsValidator.hpp>

namespace requirements::parser {

// The language parsers are declared here but defined in ModuleImpl.cpp, so the
// heavy grammar template instantiations happen in that single translation unit
// instead of in every TU that wires the module (the core and lsp modules).
std::unique_ptr<const pegium::parser::Parser>
makeRequirementsParser(const pegium::CoreServices &services);
std::unique_ptr<const pegium::parser::Parser>
makeTestsParser(const pegium::CoreServices &services);

} // namespace requirements::parser

namespace requirements::detail {

/// Applies the requirements-language core overrides to any container that
/// derives from both `pegium::CoreServices` and `RequirementsAddedServices`
/// (headless `RequirementsCoreServices` or LSP `lsp::RequirementsServices`).
///
/// Defined once here so the `core/` and `lsp/` translation units share it. The
/// parser is built through `makeRequirementsParser`, so the grammar is
/// instantiated only in `ModuleImpl.cpp`, not in every TU that wires the module.
template <typename Services>
void applyRequirementsCoreModule(Services &services) {
  services.parser = parser::makeRequirementsParser(services);
  services.languageMetaData.fileExtensions = {".req"};
  services.validator =
      std::make_unique<validation::RequirementsValidator>(services.shared);
  validation::registerRequirementsValidationChecks(services);
}

/// Applies the tests-language core overrides; see `applyRequirementsCoreModule`.
template <typename Services>
void applyTestsCoreModule(Services &services) {
  services.parser = parser::makeTestsParser(services);
  services.languageMetaData.fileExtensions = {".tst"};
  services.validator = std::make_unique<validation::TestsValidator>();
  validation::registerTestsValidationChecks(services);
}

} // namespace requirements::detail
