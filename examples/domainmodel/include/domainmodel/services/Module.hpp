#pragma once

#include <memory>

#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>

namespace domainmodel::services {

std::unique_ptr<pegium::services::Services>
create_language_services(const pegium::services::SharedServices &sharedServices,
                         std::string languageId = "domain-model");

bool register_language_services(pegium::services::SharedServices &sharedServices);

} // namespace domainmodel::services

