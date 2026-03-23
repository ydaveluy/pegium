#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <pegium/lsp/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace lsp {
class Connection;
}

namespace pegium {

/// Initializes shared services with the pieces required by an LSP server process.
void initializeSharedServicesForLanguageServer(
    SharedServices &sharedServices, ::lsp::Connection &connection);
/// Installs the default LSP feature services on one language service container.
///
/// `installDefaultCoreServices(...)` must already have been called.
void installDefaultLspServices(Services &services);
/// Installs the default shared LSP services.
///
/// `installDefaultSharedCoreServices(...)` must already have been called.
void installDefaultSharedLspServices(SharedServices &sharedServices);

} // namespace pegium

namespace pegium::services {

template <typename TServices = pegium::Services>
/// Builds one language service container with the default core and LSP services installed.
[[nodiscard]] std::unique_ptr<TServices>
makeDefaultServices(const pegium::SharedServices &sharedServices,
                    const std::string &languageId) {
  static_assert(std::is_base_of_v<pegium::Services, TServices>,
                "TServices must derive from pegium::Services.");
  static_assert(
      std::is_constructible_v<TServices, const pegium::SharedServices &>,
      "TServices must be constructible from const pegium::SharedServices&.");

  auto services = std::make_unique<TServices>(sharedServices);
  services->languageMetaData.languageId = languageId;
  installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);
  return services;
}

} // namespace pegium::services
