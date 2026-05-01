#pragma once

#include <memory>
#include <string>

#include <statemachine/core/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace statemachine {

/// Core service overrides applied on top of pegium's default core services.
void installStatemachineCoreModule(StatemachineCoreServices &services);

/// Builds the core-only statemachine language services.
std::unique_ptr<StatemachineCoreServices>
createStatemachineServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId = "statemachine");

/// Registers the core-only statemachine services in `sharedServices`.
bool registerStatemachineServices(pegium::SharedCoreServices &sharedServices);

} // namespace statemachine
