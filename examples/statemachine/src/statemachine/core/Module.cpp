#include <statemachine/core/Module.hpp>

#include <utility>

#include <statemachine/core/ModuleImpl.hpp>

namespace statemachine {

void installStatemachineCoreModule(StatemachineCoreServices &services) {
  detail::applyStatemachineCoreModule(services);
}

std::unique_ptr<StatemachineCoreServices>
createStatemachineServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId) {
  auto services = pegium::makeDefaultCoreServices<StatemachineCoreServices>(
      sharedServices, std::move(languageId));
  installStatemachineCoreModule(*services);
  return services;
}

bool registerStatemachineServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createStatemachineServices(sharedServices));
  return true;
}

} // namespace statemachine
