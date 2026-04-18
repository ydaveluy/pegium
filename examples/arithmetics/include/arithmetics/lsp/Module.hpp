#pragma once

#include <memory>
#include <string>
#include <utility>

#include <arithmetics/core/Module.hpp>
#include <arithmetics/lsp/ArithmeticsCodeActionProvider.hpp>
#include <arithmetics/lsp/ArithmeticsFormatter.hpp>
#include <arithmetics/lsp/Services.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace arithmetics::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
inline void installArithmeticsLspModule(ArithmeticsServices &services) {
  services.lsp.codeActionProvider =
      std::make_unique<ArithmeticsCodeActionProvider>();
  services.lsp.formatter = std::make_unique<ArithmeticsFormatter>(services);
}

/// Builds the LSP-enabled arithmetics language services.
inline std::unique_ptr<ArithmeticsServices>
createArithmeticsServices(const pegium::SharedServices &sharedServices,
                          std::string languageId = "arithmetics") {
  auto services = pegium::makeDefaultServices<ArithmeticsServices>(
      sharedServices, std::move(languageId));
  installArithmeticsCoreModule(*services);
  installArithmeticsLspModule(*services);
  return services;
}

/// Registers the LSP-enabled arithmetics services in `sharedServices`.
inline bool
registerArithmeticsServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createArithmeticsServices(sharedServices));
  return true;
}

} // namespace arithmetics::lsp
