#pragma once

#include <memory>
#include <string>

#include <arithmetics/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace arithmetics::lsp {

/// Builds the LSP-enabled language services for the arithmetics example.
std::unique_ptr<ArithmeticsServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId = "arithmetics");

/// Registers the LSP-enabled arithmetics services in `sharedServices`.
bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace arithmetics::lsp
