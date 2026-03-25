#pragma once

#include <statemachine/services/Services.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace statemachine::lsp {

struct StatemachineServices final : pegium::Services {
  explicit StatemachineServices(
      const pegium::SharedServices &sharedServices);
  StatemachineServices(StatemachineServices &&) noexcept;
  StatemachineServices &operator=(StatemachineServices &&) noexcept = delete;
  StatemachineServices(const StatemachineServices &) = delete;
  StatemachineServices &operator=(const StatemachineServices &) = delete;
  ~StatemachineServices() noexcept override;

  statemachine::StatemachineAddedServices statemachine;
};

[[nodiscard]] inline const StatemachineServices *
as_statemachine_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const StatemachineServices *>(&services);
}

} // namespace statemachine::lsp
