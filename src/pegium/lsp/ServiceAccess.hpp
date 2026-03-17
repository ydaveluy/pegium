#pragma once

#include <string_view>

#include <pegium/services/CoreServices.hpp>
#include <pegium/services/ServiceRegistry.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

[[nodiscard]] inline const services::Services *
as_services(const services::CoreServices *services) noexcept {
  return dynamic_cast<const services::Services *>(services);
}

[[nodiscard]] inline const services::Services *
get_services(const services::ServiceRegistry *registry,
             std::string_view languageId) noexcept {
  return registry != nullptr
             ? as_services(registry->getServicesByLanguageId(languageId))
             : nullptr;
}

[[nodiscard]] inline const services::Services *
get_services_for_uri(const services::ServiceRegistry *registry,
                     std::string_view uri) noexcept {
  return registry != nullptr ? as_services(registry->getServices(uri))
                             : nullptr;
}

[[nodiscard]] inline const services::Services *
get_services_for_file_name(const services::ServiceRegistry *registry,
                           std::string_view fileName) noexcept {
  return registry != nullptr
             ? as_services(registry->getServicesByFileName(fileName))
             : nullptr;
}

} // namespace pegium::lsp
