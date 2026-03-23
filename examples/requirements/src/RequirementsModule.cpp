#include <requirements/services/Module.hpp>

#include <requirements/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/RequirementsFormatter.hpp"
#include "validation/RequirementsValidator.hpp"
#include "validation/TestsValidator.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace requirements::services {

RequirementsLangServices::RequirementsLangServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

RequirementsLangServices::RequirementsLangServices(
    RequirementsLangServices &&) noexcept = default;

RequirementsLangServices::~RequirementsLangServices() noexcept = default;

TestsLangServices::TestsLangServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

TestsLangServices::TestsLangServices(TestsLangServices &&) noexcept = default;

TestsLangServices::~TestsLangServices() noexcept = default;

namespace {

std::unique_ptr<RequirementsLangServices>
make_requirements_services(
    const pegium::SharedServices &sharedServices,
    std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices<RequirementsLangServices>(
          sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const requirements::parser::RequirementsParser>(*services);
  services->languageMetaData.fileExtensions = {".req"};
  services->requirementsLang.validation.requirementsValidator =
      std::make_unique<validation::RequirementsValidator>();
  services->lsp.formatter = std::make_unique<lsp::RequirementsFormatter>(*services);
  validation::registerRequirementsValidationChecks(*services);
  return services;
}

std::unique_ptr<TestsLangServices>
make_tests_services(const pegium::SharedServices &sharedServices,
                    std::string languageId) {
  auto services = pegium::services::makeDefaultServices<TestsLangServices>(
      sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const requirements::parser::TestsParser>(*services);
  services->languageMetaData.fileExtensions = {".tst"};
  services->testsLang.validation.testsValidator =
      std::make_unique<validation::TestsValidator>();
  services->lsp.formatter = std::make_unique<lsp::TestsFormatter>(*services);
  validation::registerTestsValidationChecks(*services);
  return services;
}

} // namespace

std::unique_ptr<RequirementsLangServices>
create_requirements_language_services(
    const pegium::SharedServices &sharedServices,
    std::string languageId) {
  return make_requirements_services(sharedServices, std::move(languageId));
}

std::unique_ptr<TestsLangServices>
create_tests_language_services(
    const pegium::SharedServices &sharedServices,
    std::string languageId) {
  return make_tests_services(sharedServices, std::move(languageId));
}

RequirementsAndTestsLanguageServices
create_requirements_and_tests_language_services(
    const pegium::SharedServices &sharedServices) {
  return {.requirements = make_requirements_services(sharedServices,
                                                     "requirements-lang"),
          .tests = make_tests_services(sharedServices, "tests-lang")};
}

bool register_language_services(pegium::SharedServices &sharedServices) {
  auto services = create_requirements_and_tests_language_services(sharedServices);
  sharedServices.serviceRegistry->registerServices(std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements::services
