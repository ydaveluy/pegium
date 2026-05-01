#pragma once

#include <memory>

#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace domainmodel {

/// Domain-model-specific services grafted onto any pegium service container.
struct DomainModelAddedServices {
  DomainModelAddedServices() = default;
  DomainModelAddedServices(DomainModelAddedServices &&) noexcept = default;
  DomainModelAddedServices &operator=(DomainModelAddedServices &&) noexcept =
      default;
  DomainModelAddedServices(const DomainModelAddedServices &) = delete;
  DomainModelAddedServices &operator=(const DomainModelAddedServices &) = delete;
  virtual ~DomainModelAddedServices() noexcept = default;

  std::shared_ptr<const references::QualifiedNameProvider> qualifiedNameProvider;
  std::unique_ptr<validation::DomainModelValidator> validator;
};

/// Core-only language services (used by the CLI and headless scenarios).
struct DomainModelCoreServices final : pegium::CoreServices,
                                       DomainModelAddedServices {
  using pegium::CoreServices::CoreServices;
};

[[nodiscard]] inline const DomainModelCoreServices *
asDomainModelCoreServices(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const DomainModelCoreServices *>(&services);
}

[[nodiscard]] inline const DomainModelAddedServices *
asDomainModelAddedServices(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const DomainModelAddedServices *>(&services);
}

} // namespace domainmodel
