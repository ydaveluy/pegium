#pragma once

#include <memory>
#include <string>

#include <requirements/core/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace requirements {

/// Core service overrides for the requirements language.
void installRequirementsCoreModule(RequirementsCoreServices &services);

/// Core service overrides for the tests language.
void installTestsCoreModule(TestsCoreServices &services);

/// Builds the core-only requirements language services.
std::unique_ptr<RequirementsCoreServices>
createRequirementsServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId = "requirements-lang");

/// Builds the core-only tests language services.
std::unique_ptr<TestsCoreServices>
createTestsServices(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId = "tests-lang");

/// Aggregate of both languages' core services.
struct RequirementsAndTestsCoreServices {
  std::unique_ptr<RequirementsCoreServices> requirements;
  std::unique_ptr<TestsCoreServices> tests;
};

/// Builds the requirements and tests core-only language services together.
RequirementsAndTestsCoreServices createRequirementsAndTestsServices(
    const pegium::SharedCoreServices &sharedServices);

/// Registers the core-only services for both languages.
bool registerRequirementsServices(pegium::SharedCoreServices &sharedServices);

} // namespace requirements
