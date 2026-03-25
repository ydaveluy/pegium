#include <requirements/lsp/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "../core/RequirementsCoreSetup.hpp"
#include "RequirementsFormatter.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace requirements::lsp {

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
make_requirements_services(const pegium::SharedServices &sharedServices,
                           std::string languageId) {
  auto services =
      pegium::makeDefaultServices<RequirementsLangServices>(
          sharedServices, std::move(languageId));
  detail::configure_requirements_core_services(*services);
  services->lsp.formatter = std::make_unique<RequirementsFormatter>(*services);
  return services;
}

std::unique_ptr<TestsLangServices>
make_tests_services(const pegium::SharedServices &sharedServices,
                    std::string languageId) {
  auto services = pegium::makeDefaultServices<TestsLangServices>(
      sharedServices, std::move(languageId));
  detail::configure_tests_core_services(*services);
  services->lsp.formatter = std::make_unique<TestsFormatter>(*services);
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
create_tests_language_services(const pegium::SharedServices &sharedServices,
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
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements::lsp
