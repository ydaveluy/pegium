#pragma once

#include <memory>

#include <statemachine/core/validation/StatemachineValidator.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace statemachine {

/// Statemachine-specific services grafted onto any pegium service container.
/// The `unique_ptr` members already make it move-only; it needs no destructor of
/// its own — `service_cast` reaches it through the polymorphic `CoreServices`.
struct StatemachineAddedServices {
  std::unique_ptr<validation::StatemachineValidator> validator;
};

/// Core-only language services (used by the CLI and headless scenarios).
struct StatemachineCoreServices final : pegium::CoreServices,
                                        StatemachineAddedServices {
  using pegium::CoreServices::CoreServices;
};

} // namespace statemachine
