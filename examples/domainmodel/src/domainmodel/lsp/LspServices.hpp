#pragma once

#include <domainmodel/core/CoreServices.hpp>
#include <pegium/core/services/ServiceAccess.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace domainmodel {

/// LSP-enabled domain-model language services.
struct DomainModelServices final : pegium::Services,
                                   domainmodel::DomainModelAddedServices {
  using pegium::Services::Services;
};

[[nodiscard]] inline const DomainModelServices *
asDomainModelServices(const pegium::Services &services) noexcept {
  return pegium::service_cast<DomainModelServices>(services);
}

} // namespace domainmodel
