#include <domainmodel/lsp/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "../core/DomainModelCoreSetup.hpp"
#include "DomainModelFormatter.hpp"
#include "DomainModelRenameProvider.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace domainmodel::lsp {

DomainModelServices::DomainModelServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

DomainModelServices::DomainModelServices(DomainModelServices &&) noexcept = default;

DomainModelServices::~DomainModelServices() noexcept = default;

std::unique_ptr<DomainModelServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId) {
  auto services = pegium::makeDefaultServices<DomainModelServices>(
      sharedServices, std::move(languageId));
  detail::configure_core_services(*services);
  services->lsp.renameProvider =
      std::make_unique<DomainModelRenameProvider>(*services);
  services->lsp.formatter = std::make_unique<DomainModelFormatter>(*services);
  return services;
}

bool register_language_services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "domain-model"));
  return true;
}

} // namespace domainmodel::lsp
