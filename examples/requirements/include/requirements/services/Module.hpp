#pragma once

#include <memory>
#include <string>

#include <requirements/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace requirements::services {

struct RequirementsAndTestsLanguageServices {
  std::unique_ptr<RequirementsLangServices> requirements;
  std::unique_ptr<TestsLangServices> tests;
};

std::unique_ptr<RequirementsLangServices>
create_requirements_language_services(
    const pegium::SharedServices &sharedServices,
    std::string languageId = "requirements-lang");

std::unique_ptr<TestsLangServices>
create_tests_language_services(
    const pegium::SharedServices &sharedServices,
    std::string languageId = "tests-lang");

RequirementsAndTestsLanguageServices
create_requirements_and_tests_language_services(
    const pegium::SharedServices &sharedServices);

bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace requirements::services
