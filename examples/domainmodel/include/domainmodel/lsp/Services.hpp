#pragma once

#include <domainmodel/services/Services.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace domainmodel::lsp {

struct DomainModelServices final : domainmodel::DomainModelServiceAccess,
                                   pegium::Services {
  explicit DomainModelServices(
      const pegium::SharedServices &sharedServices);
  DomainModelServices(DomainModelServices &&) noexcept;
  DomainModelServices &operator=(DomainModelServices &&) noexcept = delete;
  DomainModelServices(const DomainModelServices &) = delete;
  DomainModelServices &operator=(const DomainModelServices &) = delete;
  ~DomainModelServices() noexcept override;
};

[[nodiscard]] inline const domainmodel::DomainModelServiceAccess *
as_domain_model_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const domainmodel::DomainModelServiceAccess *>(&services);
}

} // namespace domainmodel::lsp
