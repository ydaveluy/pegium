#pragma once

#include <memory>

#include <pegium/lsp/services/Services.hpp>

namespace requirements::services::validation {
class RequirementsValidator;
class TestsValidator;
}

namespace requirements::services {

struct RequirementsLangValidationServices {
  std::unique_ptr<validation::RequirementsValidator> requirementsValidator;
};

struct RequirementsLangAddedServices {
  RequirementsLangValidationServices validation;
};

struct TestsLangValidationServices {
  std::unique_ptr<validation::TestsValidator> testsValidator;
};

struct TestsLangAddedServices {
  TestsLangValidationServices validation;
};

struct RequirementsLangServices final : pegium::Services {
  explicit RequirementsLangServices(
      const pegium::SharedServices &sharedServices);
  RequirementsLangServices(RequirementsLangServices &&) noexcept;
  RequirementsLangServices &operator=(RequirementsLangServices &&) noexcept =
      delete;
  RequirementsLangServices(const RequirementsLangServices &) = delete;
  RequirementsLangServices &operator=(const RequirementsLangServices &) = delete;
  ~RequirementsLangServices() noexcept override;

  RequirementsLangAddedServices requirementsLang;
};

struct TestsLangServices final : pegium::Services {
  explicit TestsLangServices(
      const pegium::SharedServices &sharedServices);
  TestsLangServices(TestsLangServices &&) noexcept;
  TestsLangServices &operator=(TestsLangServices &&) noexcept = delete;
  TestsLangServices(const TestsLangServices &) = delete;
  TestsLangServices &operator=(const TestsLangServices &) = delete;
  ~TestsLangServices() noexcept override;

  TestsLangAddedServices testsLang;
};

[[nodiscard]] inline const RequirementsLangServices *
as_requirements_lang_services(
    const pegium::services::CoreServices &services) noexcept {
  return dynamic_cast<const RequirementsLangServices *>(&services);
}

[[nodiscard]] inline const RequirementsLangServices *
as_requirements_lang_services(
    const pegium::Services &services) noexcept {
  return dynamic_cast<const RequirementsLangServices *>(&services);
}

[[nodiscard]] inline const TestsLangServices *
as_tests_lang_services(const pegium::services::CoreServices &services) noexcept {
  return dynamic_cast<const TestsLangServices *>(&services);
}

[[nodiscard]] inline const TestsLangServices *
as_tests_lang_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const TestsLangServices *>(&services);
}

} // namespace requirements::services
