#include <requirements/lsp/Module.hpp>

#include <utility>

#include <requirements/core/validation/RequirementsValidator.hpp>
#include <requirements/core/validation/TestsValidator.hpp>
#include <requirements/lsp/RequirementsFormatter.hpp>
#include <requirements/parser/Parser.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

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

void installRequirementsCoreModule(lsp::RequirementsServices &services) {
  applyRequirementsCoreModule(services);
}

void installTestsCoreModule(lsp::TestsServices &services) {
  applyTestsCoreModule(services);
}

} // namespace requirements

namespace requirements::lsp {

void installRequirementsLspModule(RequirementsServices &services) {
  services.lsp.formatter = std::make_unique<RequirementsFormatter>(services);
}

void installTestsLspModule(TestsServices &services) {
  services.lsp.formatter = std::make_unique<TestsFormatter>(services);
}

std::unique_ptr<RequirementsServices>
createRequirementsServices(const pegium::SharedServices &sharedServices,
                           std::string languageId) {
  auto services = pegium::makeDefaultServices<RequirementsServices>(
      sharedServices, std::move(languageId));
  requirements::installRequirementsCoreModule(*services);
  installRequirementsLspModule(*services);
  return services;
}

std::unique_ptr<TestsServices>
createTestsServices(const pegium::SharedServices &sharedServices,
                    std::string languageId) {
  auto services = pegium::makeDefaultServices<TestsServices>(
      sharedServices, std::move(languageId));
  requirements::installTestsCoreModule(*services);
  installTestsLspModule(*services);
  return services;
}

RequirementsAndTestsServices
createRequirementsAndTestsServices(const pegium::SharedServices &sharedServices) {
  return {.requirements = createRequirementsServices(sharedServices),
          .tests = createTestsServices(sharedServices)};
}

bool registerRequirementsServices(pegium::SharedServices &sharedServices) {
  auto services = createRequirementsAndTestsServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements::lsp
