#pragma once

#include <memory>
#include <string>

#include <domainmodel/services/Services.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace domainmodel {

/// Builds the core language services for the domainmodel example.
std::unique_ptr<DomainModelServices>
create_language_services(
    const pegium::SharedCoreServices &sharedServices,
                         std::string languageId = "domain-model");

/// Registers the domainmodel core language services in `sharedServices`.
bool register_language_services(
    pegium::SharedCoreServices &sharedServices);

} // namespace domainmodel
