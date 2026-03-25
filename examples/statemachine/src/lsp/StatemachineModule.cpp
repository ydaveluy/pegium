#include <statemachine/lsp/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "../core/StatemachineCoreSetup.hpp"
#include "StatemachineFormatter.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace statemachine::lsp {

StatemachineServices::StatemachineServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

StatemachineServices::StatemachineServices(StatemachineServices &&) noexcept =
    default;

StatemachineServices::~StatemachineServices() noexcept = default;

std::unique_ptr<StatemachineServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId) {
  auto services =
      pegium::makeDefaultServices<StatemachineServices>(
          sharedServices, std::move(languageId));
  detail::configure_core_services(*services);
  services->lsp.formatter = std::make_unique<StatemachineFormatter>(*services);
  return services;
}

bool register_language_services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "statemachine"));
  return true;
}

} // namespace statemachine::lsp
