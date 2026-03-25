#pragma once

#include <memory>

#include <pegium/core/services/CoreServices.hpp>

namespace statemachine::validation {
class StatemachineValidator;
}

namespace statemachine {

struct StatemachineValidationServices {
  std::unique_ptr<validation::StatemachineValidator> statemachineValidator;
};

struct StatemachineAddedServices {
  StatemachineValidationServices validation;
};

struct StatemachineServices final : pegium::CoreServices {
  explicit StatemachineServices(
      const pegium::SharedCoreServices &sharedServices);
  StatemachineServices(StatemachineServices &&) noexcept;
  StatemachineServices &operator=(StatemachineServices &&) noexcept = delete;
  StatemachineServices(const StatemachineServices &) = delete;
  StatemachineServices &operator=(const StatemachineServices &) = delete;
  ~StatemachineServices() noexcept override;

  StatemachineAddedServices statemachine;
};

[[nodiscard]] inline const StatemachineServices *
as_statemachine_services(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const StatemachineServices *>(&services);
}

} // namespace statemachine
