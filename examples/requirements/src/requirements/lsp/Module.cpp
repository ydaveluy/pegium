#include <requirements/lsp/Module.hpp>

#include <utility>

#include <requirements/core/ModuleImpl.hpp>
#include <requirements/lsp/RequirementsFormatter.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace requirements {

void installRequirementsCoreModule(lsp::RequirementsServices &services) {
  detail::applyRequirementsCoreModule(services);
}

void installTestsCoreModule(lsp::TestsServices &services) {
  detail::applyTestsCoreModule(services);
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
