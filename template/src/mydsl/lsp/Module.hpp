#pragma once

#include <memory>
#include <string>

#include <mydsl/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace mydsl {

/// Core service overrides applied on top of pegium's default core services,
/// targeting the LSP-enabled service container.
void installMyDslCoreModule(mydsl::lsp::MyDslServices &services);

} // namespace mydsl

namespace mydsl::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
void installMyDslLspModule(MyDslServices &services);

/// Builds the LSP-enabled language services.
std::unique_ptr<MyDslServices>
createMyDslServices(const pegium::SharedServices &sharedServices,
                    std::string languageId = "mydsl");

/// Registers the LSP-enabled services in `sharedServices`.
bool registerMyDslServices(pegium::SharedServices &sharedServices);

} // namespace mydsl::lsp
