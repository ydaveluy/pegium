#pragma once

#include <memory>
#include <string>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@ {

/// Builds the core-only @PEGIUM_NEW_CLASS@ language services.
std::unique_ptr<@PEGIUM_NEW_CLASS@CoreServices>
create@PEGIUM_NEW_CLASS@Services(const pegium::SharedCoreServices &sharedServices,
                     std::string languageId = "@PEGIUM_NEW_LANGUAGE_ID@");

/// Registers the core-only @PEGIUM_NEW_CLASS@ services in `sharedServices`.
bool register@PEGIUM_NEW_CLASS@Services(pegium::SharedCoreServices &sharedServices);

} // namespace @PEGIUM_NEW_LANGUAGE_ID@
