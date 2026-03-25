#include <statemachine/services/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "StatemachineCoreSetup.hpp"

namespace statemachine {

StatemachineServices::StatemachineServices(
    const pegium::SharedCoreServices &sharedServices)
    : pegium::CoreServices(sharedServices) {}

StatemachineServices::StatemachineServices(StatemachineServices &&) noexcept = default;

StatemachineServices::~StatemachineServices() noexcept = default;

std::unique_ptr<StatemachineServices>
create_language_services(
    const pegium::SharedCoreServices &sharedServices,
                         std::string languageId) {
  auto services = std::make_unique<StatemachineServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  detail::configure_core_services(*services);
  return services;
}

bool register_language_services(
    pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "statemachine"));
  return true;
}

} // namespace statemachine
