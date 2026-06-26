#pragma once

#include <memory>
#include <string>

#include <@PEGIUM_NEW_LANGUAGE_ID@/lsp/LspServices.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@ {

/// Builds the LSP-enabled @PEGIUM_NEW_CLASS@ language services.
std::unique_ptr<@PEGIUM_NEW_CLASS@Services>
create@PEGIUM_NEW_CLASS@LspServices(const pegium::SharedServices &sharedServices,
                     std::string languageId = "@PEGIUM_NEW_LANGUAGE_ID@");

/// Registers the LSP-enabled @PEGIUM_NEW_CLASS@ services in `sharedServices`.
bool register@PEGIUM_NEW_CLASS@LspServices(pegium::SharedServices &sharedServices);

} // namespace @PEGIUM_NEW_LANGUAGE_ID@
