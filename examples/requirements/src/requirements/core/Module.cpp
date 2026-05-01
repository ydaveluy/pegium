#include <requirements/core/Module.hpp>

#include <utility>

#include <requirements/core/validation/RequirementsValidator.hpp>
#include <requirements/core/validation/TestsValidator.hpp>
#include <requirements/parser/Parser.hpp>

namespace requirements {

namespace {
template <typename Services>
void applyRequirementsCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::RequirementsParser>(services);
  services.languageMetaData.fileExtensions = {".req"};
  services.validator = std::make_unique<validation::RequirementsValidator>();
  validation::registerRequirementsValidationChecks(services);
}

template <typename Services>
void applyTestsCoreModule(Services &services) {
  services.parser = std::make_unique<const parser::TestsParser>(services);
  services.languageMetaData.fileExtensions = {".tst"};
  services.validator = std::make_unique<validation::TestsValidator>();
  validation::registerTestsValidationChecks(services);
}
} // namespace

void installRequirementsCoreModule(RequirementsCoreServices &services) {
  applyRequirementsCoreModule(services);
}

void installTestsCoreModule(TestsCoreServices &services) {
  applyTestsCoreModule(services);
}

std::unique_ptr<RequirementsCoreServices>
createRequirementsServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId) {
  auto services = std::make_unique<RequirementsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installRequirementsCoreModule(*services);
  return services;
}

std::unique_ptr<TestsCoreServices>
createTestsServices(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId) {
  auto services = std::make_unique<TestsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installTestsCoreModule(*services);
  return services;
}

RequirementsAndTestsCoreServices createRequirementsAndTestsServices(
    const pegium::SharedCoreServices &sharedServices) {
  return {.requirements = createRequirementsServices(sharedServices),
          .tests = createTestsServices(sharedServices)};
}

bool registerRequirementsServices(pegium::SharedCoreServices &sharedServices) {
  auto services = createRequirementsAndTestsServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements
