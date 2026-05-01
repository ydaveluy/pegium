#pragma once

#include <memory>
#include <string>

#include <requirements/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace requirements {

/// Core service overrides (LSP-enabled bundle overload).
void installRequirementsCoreModule(requirements::lsp::RequirementsServices &services);
/// Core service overrides for the tests language (LSP-enabled bundle overload).
void installTestsCoreModule(requirements::lsp::TestsServices &services);

} // namespace requirements

namespace requirements::lsp {

/// LSP service overrides for the requirements language.
void installRequirementsLspModule(RequirementsServices &services);

/// LSP service overrides for the tests language.
void installTestsLspModule(TestsServices &services);

/// Builds the LSP-enabled requirements language services.
std::unique_ptr<RequirementsServices>
createRequirementsServices(const pegium::SharedServices &sharedServices,
                           std::string languageId = "requirements-lang");

/// Builds the LSP-enabled tests language services.
std::unique_ptr<TestsServices>
createTestsServices(const pegium::SharedServices &sharedServices,
                    std::string languageId = "tests-lang");

/// Aggregate of both languages' LSP-enabled services.
struct RequirementsAndTestsServices {
  std::unique_ptr<RequirementsServices> requirements;
  std::unique_ptr<TestsServices> tests;
};

/// Builds the LSP-enabled requirements and tests language services together.
RequirementsAndTestsServices
createRequirementsAndTestsServices(const pegium::SharedServices &sharedServices);

/// Registers the LSP-enabled services for both languages.
bool registerRequirementsServices(pegium::SharedServices &sharedServices);

} // namespace requirements::lsp
