#include <requirements/services/Module.hpp>

#include <requirements/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/RequirementsFormatter.hpp"
#include "validation/RequirementsValidator.hpp"
#include "validation/TestsValidator.hpp"

namespace requirements::services {

namespace {

std::unique_ptr<pegium::services::Services>
make_requirements_services(const pegium::services::SharedServices &sharedServices,
                           std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const requirements::parser::RequirementsParser>(*services);
  services->languageMetaData.fileExtensions = {".req"};
  services->lsp.formatter = std::make_unique<lsp::RequirementsFormatter>(*services);
  validation::RequirementsValidator::registerValidationChecks(
      *services->validation.validationRegistry, *services);
  return services;
}

std::unique_ptr<pegium::services::Services>
make_tests_services(const pegium::services::SharedServices &sharedServices,
                    std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const requirements::parser::TestsParser>(*services);
  services->languageMetaData.fileExtensions = {".tst"};
  services->lsp.formatter = std::make_unique<lsp::TestsFormatter>(*services);
  validation::TestsValidator::registerValidationChecks(
      *services->validation.validationRegistry, *services);
  return services;
}

} // namespace

std::unique_ptr<pegium::services::Services>
create_requirements_language_services(
    const pegium::services::SharedServices &sharedServices,
    std::string languageId) {
  return make_requirements_services(sharedServices, std::move(languageId));
}

std::unique_ptr<pegium::services::Services>
create_tests_language_services(
    const pegium::services::SharedServices &sharedServices,
    std::string languageId) {
  return make_tests_services(sharedServices, std::move(languageId));
}

bool register_language_services(pegium::services::SharedServices &sharedServices) {
  if (!sharedServices.serviceRegistry->registerServices(
          make_requirements_services(sharedServices, "requirements-lang"))) {
    return false;
  }
  if (!sharedServices.serviceRegistry->registerServices(
          make_tests_services(sharedServices, "tests-lang"))) {
    return false;
  }
  return true;
}

} // namespace requirements::services
