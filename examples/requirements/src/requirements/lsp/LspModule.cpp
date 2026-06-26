#include <requirements/lsp/LspModule.hpp>

#include <utility>

#include <requirements/core/CoreModule.hpp>
#include <requirements/lsp/RequirementsFormatter.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace requirements {

void installRequirementsLspModule(RequirementsServices &services) {
  services.lsp.formatter = std::make_unique<RequirementsFormatter>(services);
}

void installTestsLspModule(TestsServices &services) {
  services.lsp.formatter = std::make_unique<TestsFormatter>(services);
}

std::unique_ptr<RequirementsServices>
createRequirementsLspServices(const pegium::SharedServices &sharedServices,
                              std::string languageId) {
  auto services = pegium::makeDefaultServices<RequirementsServices>(
      sharedServices, std::move(languageId));
  // The container is-a CoreServices and is-a RequirementsAddedServices, so it
  // binds both parameters of the (non-template) core wiring.
  installRequirementsCoreModule(*services, *services);
  installRequirementsLspModule(*services);
  return services;
}

std::unique_ptr<TestsServices>
createTestsLspServices(const pegium::SharedServices &sharedServices,
                       std::string languageId) {
  auto services = pegium::makeDefaultServices<TestsServices>(
      sharedServices, std::move(languageId));
  installTestsCoreModule(*services, *services);
  installTestsLspModule(*services);
  return services;
}

RequirementsAndTestsServices
createRequirementsAndTestsLspServices(const pegium::SharedServices &sharedServices) {
  return {.requirements = createRequirementsLspServices(sharedServices),
          .tests = createTestsLspServices(sharedServices)};
}

bool registerRequirementsLspServices(pegium::SharedServices &sharedServices) {
  auto services = createRequirementsAndTestsLspServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements
