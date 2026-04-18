#pragma once

#include <domainmodel/core/Services.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace domainmodel::lsp {

/// LSP-enabled domain-model language services.
struct DomainModelServices final : pegium::Services,
                                   domainmodel::DomainModelAddedServices {
  using pegium::Services::Services;
};

[[nodiscard]] inline const DomainModelServices *
asDomainModelServices(const pegium::Services &services) noexcept {
  return dynamic_cast<const DomainModelServices *>(&services);
}

} // namespace domainmodel::lsp
