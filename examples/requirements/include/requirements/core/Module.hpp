#pragma once

#include <memory>
#include <string>
#include <utility>

#include <requirements/core/Services.hpp>
#include <requirements/parser/Parser.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace requirements {

/// Core service overrides for the requirements language.
template <typename Services>
void installRequirementsCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::RequirementsParser>(services);
  services.languageMetaData.fileExtensions = {".req"};
  services.validator = std::make_unique<validation::RequirementsValidator>();
  validation::registerRequirementsValidationChecks(services);
}

/// Core service overrides for the tests language.
template <typename Services>
void installTestsCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::TestsParser>(services);
  services.languageMetaData.fileExtensions = {".tst"};
  services.validator = std::make_unique<validation::TestsValidator>();
  validation::registerTestsValidationChecks(services);
}

/// Builds the core-only requirements language services.
inline std::unique_ptr<RequirementsCoreServices>
createRequirementsServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId = "requirements-lang") {
  auto services = std::make_unique<RequirementsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installRequirementsCoreModule(*services);
  return services;
}

/// Builds the core-only tests language services.
inline std::unique_ptr<TestsCoreServices>
createTestsServices(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId = "tests-lang") {
  auto services = std::make_unique<TestsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installTestsCoreModule(*services);
  return services;
}

/// Aggregate of both languages' core services.
struct RequirementsAndTestsCoreServices {
  std::unique_ptr<RequirementsCoreServices> requirements;
  std::unique_ptr<TestsCoreServices> tests;
};

/// Builds the requirements and tests core-only language services together.
inline RequirementsAndTestsCoreServices
createRequirementsAndTestsServices(
    const pegium::SharedCoreServices &sharedServices) {
  return {.requirements = createRequirementsServices(sharedServices),
          .tests = createTestsServices(sharedServices)};
}

/// Registers the core-only services for both languages.
inline bool
registerRequirementsServices(pegium::SharedCoreServices &sharedServices) {
  auto services = createRequirementsAndTestsServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements
