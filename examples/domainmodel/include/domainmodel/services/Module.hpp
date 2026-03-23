#pragma once

#include <memory>
#include <string>

#include <domainmodel/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace domainmodel::services {

std::unique_ptr<DomainModelServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId = "domain-model");

bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace domainmodel::services
