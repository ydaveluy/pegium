#include <requirements/core/Module.hpp>

#include <utility>

#include <requirements/core/ModuleImpl.hpp>

namespace requirements {

void installRequirementsCoreModule(RequirementsCoreServices &services) {
  detail::applyRequirementsCoreModule(services);
}

void installTestsCoreModule(TestsCoreServices &services) {
  detail::applyTestsCoreModule(services);
}

std::unique_ptr<RequirementsCoreServices>
createRequirementsServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId) {
  auto services = pegium::makeDefaultCoreServices<RequirementsCoreServices>(
      sharedServices, std::move(languageId));
  installRequirementsCoreModule(*services);
  return services;
}

std::unique_ptr<TestsCoreServices>
createTestsServices(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId) {
  auto services = pegium::makeDefaultCoreServices<TestsCoreServices>(
      sharedServices, std::move(languageId));
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
