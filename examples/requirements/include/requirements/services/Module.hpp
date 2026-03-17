#pragma once

#include <memory>

#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>

namespace requirements::services {

std::unique_ptr<pegium::services::Services>
create_requirements_language_services(
    const pegium::services::SharedServices &sharedServices,
    std::string languageId = "requirements-lang");

std::unique_ptr<pegium::services::Services>
create_tests_language_services(
    const pegium::services::SharedServices &sharedServices,
    std::string languageId = "tests-lang");

bool register_language_services(pegium::services::SharedServices &sharedServices);

} // namespace requirements::services

