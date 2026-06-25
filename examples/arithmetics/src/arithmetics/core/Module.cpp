#include <arithmetics/core/Module.hpp>

#include <utility>

#include <arithmetics/core/ModuleImpl.hpp>

namespace arithmetics {

void installArithmeticsCoreModule(ArithmeticsCoreServices &services) {
  detail::applyArithmeticsCoreModule(services);
}

std::unique_ptr<ArithmeticsCoreServices>
createArithmeticsServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId) {
  auto services = pegium::makeDefaultCoreServices<ArithmeticsCoreServices>(
      sharedServices, std::move(languageId));
  installArithmeticsCoreModule(*services);
  return services;
}

bool registerArithmeticsServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createArithmeticsServices(sharedServices));
  return true;
}

} // namespace arithmetics
