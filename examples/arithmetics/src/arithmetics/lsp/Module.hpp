#pragma once

#include <memory>
#include <string>

#include <arithmetics/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace arithmetics {

/// Core service overrides applied on top of pegium's default core services,
/// overload targeting the LSP-enabled service bundle.
void installArithmeticsCoreModule(arithmetics::lsp::ArithmeticsServices &services);

} // namespace arithmetics

namespace arithmetics::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
void installArithmeticsLspModule(ArithmeticsServices &services);

/// Builds the LSP-enabled arithmetics language services.
std::unique_ptr<ArithmeticsServices>
createArithmeticsServices(const pegium::SharedServices &sharedServices,
                          std::string languageId = "arithmetics");

/// Registers the LSP-enabled arithmetics services in `sharedServices`.
bool registerArithmeticsServices(pegium::SharedServices &sharedServices);

} // namespace arithmetics::lsp
