#pragma once

#include <memory>

#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace domainmodel {

/// Domain-model-specific services grafted onto any pegium service container.
/// The `unique_ptr` members already make it move-only; it needs no destructor of
/// its own — `service_cast` reaches it through the polymorphic `CoreServices`.
struct DomainModelAddedServices {
  std::shared_ptr<const references::QualifiedNameProvider> qualifiedNameProvider;
  std::unique_ptr<validation::DomainModelValidator> validator;
};

/// Core-only language services (used by the CLI and headless scenarios).
struct DomainModelCoreServices final : pegium::CoreServices,
                                       DomainModelAddedServices {
  using pegium::CoreServices::CoreServices;
};

} // namespace domainmodel
