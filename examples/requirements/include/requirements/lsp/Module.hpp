#pragma once

#include <memory>
#include <string>
#include <utility>

#include <requirements/core/Module.hpp>
#include <requirements/lsp/RequirementsFormatter.hpp>
#include <requirements/lsp/Services.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace requirements::lsp {

/// LSP service overrides for the requirements language.
inline void installRequirementsLspModule(RequirementsServices &services) {
  services.lsp.formatter = std::make_unique<RequirementsFormatter>(services);
}

/// LSP service overrides for the tests language.
inline void installTestsLspModule(TestsServices &services) {
  services.lsp.formatter = std::make_unique<TestsFormatter>(services);
}

/// Builds the LSP-enabled requirements language services.
inline std::unique_ptr<RequirementsServices>
createRequirementsServices(const pegium::SharedServices &sharedServices,
                           std::string languageId = "requirements-lang") {
  auto services = pegium::makeDefaultServices<RequirementsServices>(
      sharedServices, std::move(languageId));
  installRequirementsCoreModule(*services);
  installRequirementsLspModule(*services);
  return services;
}

/// Builds the LSP-enabled tests language services.
inline std::unique_ptr<TestsServices>
createTestsServices(const pegium::SharedServices &sharedServices,
                    std::string languageId = "tests-lang") {
  auto services = pegium::makeDefaultServices<TestsServices>(
      sharedServices, std::move(languageId));
  installTestsCoreModule(*services);
  installTestsLspModule(*services);
  return services;
}

/// Aggregate of both languages' LSP-enabled services.
struct RequirementsAndTestsServices {
  std::unique_ptr<RequirementsServices> requirements;
  std::unique_ptr<TestsServices> tests;
};

/// Builds the LSP-enabled requirements and tests language services together.
inline RequirementsAndTestsServices
createRequirementsAndTestsServices(const pegium::SharedServices &sharedServices) {
  return {.requirements = createRequirementsServices(sharedServices),
          .tests = createTestsServices(sharedServices)};
}

/// Registers the LSP-enabled services for both languages.
inline bool
registerRequirementsServices(pegium::SharedServices &sharedServices) {
  auto services = createRequirementsAndTestsServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements::lsp
