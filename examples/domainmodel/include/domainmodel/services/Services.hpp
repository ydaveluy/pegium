#pragma once

#include <memory>

#include <pegium/lsp/services/Services.hpp>

namespace domainmodel::services::references {
class QualifiedNameProvider;
}

namespace domainmodel::services::validation {
class DomainModelValidator;
}

namespace domainmodel::services {

struct DomainModelReferenceServices {
  std::shared_ptr<const references::QualifiedNameProvider> qualifiedNameProvider;
};

struct DomainModelValidationServices {
  std::unique_ptr<validation::DomainModelValidator> domainModelValidator;
};

struct DomainModelAddedServices {
  DomainModelReferenceServices references;
  DomainModelValidationServices validation;
};

struct DomainModelServices final : pegium::Services {
  explicit DomainModelServices(
      const pegium::SharedServices &sharedServices);
  DomainModelServices(DomainModelServices &&) noexcept;
  DomainModelServices &operator=(DomainModelServices &&) noexcept = delete;
  DomainModelServices(const DomainModelServices &) = delete;
  DomainModelServices &operator=(const DomainModelServices &) = delete;
  ~DomainModelServices() noexcept override;

  DomainModelAddedServices domainModel;
};

[[nodiscard]] inline const DomainModelServices *
as_domain_model_services(const pegium::services::CoreServices &services) noexcept {
  return dynamic_cast<const DomainModelServices *>(&services);
}

[[nodiscard]] inline const DomainModelServices *
as_domain_model_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const DomainModelServices *>(&services);
}

} // namespace domainmodel::services
