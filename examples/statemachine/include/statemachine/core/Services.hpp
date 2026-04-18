#pragma once

#include <memory>

#include <statemachine/core/validation/StatemachineValidator.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace statemachine {

/// Statemachine-specific services grafted onto any pegium service container.
struct StatemachineAddedServices {
  std::unique_ptr<validation::StatemachineValidator> validator;
};

/// Core-only language services (used by the CLI and headless scenarios).
struct StatemachineCoreServices final : pegium::CoreServices,
                                        StatemachineAddedServices {
  using pegium::CoreServices::CoreServices;
};

[[nodiscard]] inline const StatemachineCoreServices *
asStatemachineCoreServices(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const StatemachineCoreServices *>(&services);
}

} // namespace statemachine
