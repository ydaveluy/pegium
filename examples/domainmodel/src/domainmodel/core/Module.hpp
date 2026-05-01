#pragma once

#include <memory>
#include <string>

#include <domainmodel/core/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace domainmodel {

/// Core service overrides applied on top of pegium's default core services.
void installDomainModelCoreModule(DomainModelCoreServices &services);

/// Builds the core-only domain-model language services.
std::unique_ptr<DomainModelCoreServices>
createDomainModelServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId = "domain-model");

/// Registers the core-only domain-model services in `sharedServices`.
bool registerDomainModelServices(pegium::SharedCoreServices &sharedServices);

} // namespace domainmodel
