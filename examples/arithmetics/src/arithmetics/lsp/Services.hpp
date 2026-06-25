#pragma once

#include <arithmetics/core/Services.hpp>
#include <pegium/core/services/ServiceAccess.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace arithmetics::lsp {

/// LSP-enabled arithmetics language services.
struct ArithmeticsServices final : pegium::Services,
                                   arithmetics::ArithmeticsAddedServices {
  using pegium::Services::Services;
};

[[nodiscard]] inline const ArithmeticsServices *
asArithmeticsServices(const pegium::Services &services) noexcept {
  return pegium::service_cast<ArithmeticsServices>(services);
}

} // namespace arithmetics::lsp
