#pragma once

#include <string_view>

#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/ServiceAccess.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>

namespace pegium {

/// Downcasts a core service container to the LSP-aware `pegium::Services` type.
///
/// Thin specialization of the generic `service_cast` for the framework's own
/// `CoreServices` -> `Services` pair; accepts (and tolerates) a null pointer.
[[nodiscard]] inline const Services *
as_services(const pegium::CoreServices *services) noexcept {
  return services != nullptr ? service_cast<Services>(*services) : nullptr;
}

/// Resolves a URI to LSP-aware services, returning `nullptr` on failure.
///
/// `registry` must refer to a live service registry. Not `noexcept`:
/// `findServices(...)` may throw while normalizing the URI (e.g. filesystem or
/// resource-exhaustion failure).
[[nodiscard]] inline const Services *
get_services(const pegium::ServiceRegistry &registry, std::string_view uri) {
  return as_services(registry.findServices(uri));
}

} // namespace pegium
