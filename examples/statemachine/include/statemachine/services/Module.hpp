#pragma once

#include <memory>
#include <string>

#include <statemachine/services/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace statemachine {

/// Builds the core language services for the statemachine example.
std::unique_ptr<StatemachineServices>
create_language_services(
    const pegium::SharedCoreServices &sharedServices,
                         std::string languageId = "statemachine");

/// Registers the statemachine core language services in `sharedServices`.
bool register_language_services(
    pegium::SharedCoreServices &sharedServices);

} // namespace statemachine
