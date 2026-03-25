#include <requirements/services/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "RequirementsCoreSetup.hpp"

namespace requirements {

RequirementsLangServices::RequirementsLangServices(
    const pegium::SharedCoreServices &sharedServices)
    : pegium::CoreServices(sharedServices) {}

RequirementsLangServices::RequirementsLangServices(
    RequirementsLangServices &&) noexcept = default;

RequirementsLangServices::~RequirementsLangServices() noexcept = default;

TestsLangServices::TestsLangServices(
    const pegium::SharedCoreServices &sharedServices)
    : pegium::CoreServices(sharedServices) {}

TestsLangServices::TestsLangServices(TestsLangServices &&) noexcept = default;

TestsLangServices::~TestsLangServices() noexcept = default;

namespace {

std::unique_ptr<RequirementsLangServices>
make_requirements_services(
    const pegium::SharedCoreServices &sharedServices,
    std::string languageId) {
  auto services = std::make_unique<RequirementsLangServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  detail::configure_requirements_core_services(*services);
  return services;
}

std::unique_ptr<TestsLangServices>
make_tests_services(const pegium::SharedCoreServices &sharedServices,
                    std::string languageId) {
  auto services = std::make_unique<TestsLangServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  detail::configure_tests_core_services(*services);
  return services;
}

} // namespace

std::unique_ptr<RequirementsLangServices>
create_requirements_language_services(
    const pegium::SharedCoreServices &sharedServices,
    std::string languageId) {
  return make_requirements_services(sharedServices, std::move(languageId));
}

std::unique_ptr<TestsLangServices>
create_tests_language_services(
    const pegium::SharedCoreServices &sharedServices,
    std::string languageId) {
  return make_tests_services(sharedServices, std::move(languageId));
}

RequirementsAndTestsLanguageServices
create_requirements_and_tests_language_services(
    const pegium::SharedCoreServices &sharedServices) {
  return {.requirements = make_requirements_services(sharedServices,
                                                     "requirements-lang"),
          .tests = make_tests_services(sharedServices, "tests-lang")};
}

bool register_language_services(
    pegium::SharedCoreServices &sharedServices) {
  auto services = create_requirements_and_tests_language_services(sharedServices);
  sharedServices.serviceRegistry->registerServices(std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}

} // namespace requirements
