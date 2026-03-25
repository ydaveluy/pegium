#pragma once

#include <memory>

#include <pegium/core/services/CoreServices.hpp>

namespace domainmodel::references {
class QualifiedNameProvider;
}

namespace domainmodel::validation {
class DomainModelValidator;
}

namespace domainmodel {

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

struct DomainModelServiceAccess {
  DomainModelServiceAccess() = default;
  DomainModelServiceAccess(DomainModelServiceAccess &&) noexcept = default;
  DomainModelServiceAccess &operator=(DomainModelServiceAccess &&) noexcept =
      default;
  DomainModelServiceAccess(const DomainModelServiceAccess &) = delete;
  DomainModelServiceAccess &operator=(const DomainModelServiceAccess &) = delete;
  virtual ~DomainModelServiceAccess() noexcept = default;

  DomainModelAddedServices domainModel;
};

struct DomainModelServices final : DomainModelServiceAccess,
                                   pegium::CoreServices {
  explicit DomainModelServices(
      const pegium::SharedCoreServices &sharedServices);
  DomainModelServices(DomainModelServices &&) noexcept;
  DomainModelServices &operator=(DomainModelServices &&) noexcept = delete;
  DomainModelServices(const DomainModelServices &) = delete;
  DomainModelServices &operator=(const DomainModelServices &) = delete;
  ~DomainModelServices() noexcept override;
};

[[nodiscard]] inline const DomainModelServiceAccess *
as_domain_model_services(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const DomainModelServiceAccess *>(&services);
}

} // namespace domainmodel
