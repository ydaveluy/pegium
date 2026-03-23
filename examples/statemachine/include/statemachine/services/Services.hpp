#pragma once

#include <memory>

#include <pegium/lsp/services/Services.hpp>

namespace statemachine::services::validation {
class StatemachineValidator;
}

namespace statemachine::services {

struct StatemachineValidationServices {
  std::unique_ptr<validation::StatemachineValidator> statemachineValidator;
};

struct StatemachineAddedServices {
  StatemachineValidationServices validation;
};

struct StatemachineServices final : pegium::Services {
  explicit StatemachineServices(
      const pegium::SharedServices &sharedServices);
  StatemachineServices(StatemachineServices &&) noexcept;
  StatemachineServices &operator=(StatemachineServices &&) noexcept = delete;
  StatemachineServices(const StatemachineServices &) = delete;
  StatemachineServices &operator=(const StatemachineServices &) = delete;
  ~StatemachineServices() noexcept override;

  StatemachineAddedServices statemachine;
};

[[nodiscard]] inline const StatemachineServices *
as_statemachine_services(const pegium::services::CoreServices &services) noexcept {
  return dynamic_cast<const StatemachineServices *>(&services);
}

[[nodiscard]] inline const StatemachineServices *
as_statemachine_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const StatemachineServices *>(&services);
}

} // namespace statemachine::services
