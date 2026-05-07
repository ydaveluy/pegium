#pragma once

#include <memory>
#include <string>

#include <mydsl/core/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace mydsl {

/// Core service overrides applied on top of pegium's default core services.
void installMyDslCoreModule(MyDslCoreServices &services);

/// Builds the core-only language services.
std::unique_ptr<MyDslCoreServices>
createMyDslServices(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId = "mydsl");

/// Registers the core-only services in `sharedServices`.
bool registerMyDslServices(pegium::SharedCoreServices &sharedServices);

} // namespace mydsl
