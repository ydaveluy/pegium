#pragma once

#include <memory>

#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>

namespace arithmetics::services {

std::unique_ptr<pegium::services::Services>
create_language_services(const pegium::services::SharedServices &sharedServices,
                         std::string languageId);

bool register_language_services(pegium::services::SharedServices &sharedServices);

} // namespace arithmetics::services

