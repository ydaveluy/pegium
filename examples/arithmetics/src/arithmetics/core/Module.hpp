#pragma once

#include <memory>
#include <string>

#include <arithmetics/core/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace arithmetics {

/// Core service overrides applied on top of pegium's default core services.
void installArithmeticsCoreModule(ArithmeticsCoreServices &services);

/// Builds the core-only arithmetics language services.
std::unique_ptr<ArithmeticsCoreServices>
createArithmeticsServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId = "arithmetics");

/// Registers the core-only arithmetics services in `sharedServices`.
bool registerArithmeticsServices(pegium::SharedCoreServices &sharedServices);

} // namespace arithmetics
