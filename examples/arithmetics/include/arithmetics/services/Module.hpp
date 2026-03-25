#pragma once

#include <memory>
#include <string>

#include <arithmetics/services/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace arithmetics {

/// Builds the core language services for the arithmetics example.
std::unique_ptr<ArithmeticsServices>
create_language_services(
    const pegium::SharedCoreServices &sharedServices,
    std::string languageId = "arithmetics");

/// Registers the arithmetics core language services in `sharedServices`.
bool register_language_services(
    pegium::SharedCoreServices &sharedServices);

} // namespace arithmetics
