#pragma once

#include <statemachine/core/CoreServices.hpp>
#include <pegium/core/services/ServiceAccess.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace statemachine {

/// LSP-enabled statemachine language services.
struct StatemachineServices final : pegium::Services,
                                    statemachine::StatemachineAddedServices {
  using pegium::Services::Services;
};

[[nodiscard]] inline const StatemachineServices *
asStatemachineServices(const pegium::Services &services) noexcept {
  return pegium::service_cast<StatemachineServices>(services);
}

} // namespace statemachine
