#pragma once

#include <string_view>

#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>

namespace pegium {

/// Downcasts a core service container to the LSP-aware `pegium::Services` type.
[[nodiscard]] inline const Services *
as_services(const services::CoreServices *services) noexcept {
  return dynamic_cast<const Services *>(services);
}

/// Resolves a URI to LSP-aware services, returning `nullptr` on failure.
///
/// `registry` must refer to a live service registry.
[[nodiscard]] inline const Services *
get_services(const services::ServiceRegistry &registry,
             std::string_view uri) noexcept {
  return as_services(registry.findServices(uri));
}

} // namespace pegium
