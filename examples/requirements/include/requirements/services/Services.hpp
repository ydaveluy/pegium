#pragma once

#include <memory>

#include <pegium/core/services/CoreServices.hpp>

namespace requirements::validation {
class RequirementsValidator;
class TestsValidator;
}

namespace requirements {

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

struct RequirementsLangServices final : pegium::CoreServices {
  explicit RequirementsLangServices(
      const pegium::SharedCoreServices &sharedServices);
  RequirementsLangServices(RequirementsLangServices &&) noexcept;
  RequirementsLangServices &operator=(RequirementsLangServices &&) noexcept =
      delete;
  RequirementsLangServices(const RequirementsLangServices &) = delete;
  RequirementsLangServices &operator=(const RequirementsLangServices &) = delete;
  ~RequirementsLangServices() noexcept override;

  RequirementsLangAddedServices requirementsLang;
};

struct TestsLangServices final : pegium::CoreServices {
  explicit TestsLangServices(
      const pegium::SharedCoreServices &sharedServices);
  TestsLangServices(TestsLangServices &&) noexcept;
  TestsLangServices &operator=(TestsLangServices &&) noexcept = delete;
  TestsLangServices(const TestsLangServices &) = delete;
  TestsLangServices &operator=(const TestsLangServices &) = delete;
  ~TestsLangServices() noexcept override;

  TestsLangAddedServices testsLang;
};

[[nodiscard]] inline const RequirementsLangServices *
as_requirements_lang_services(
    const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const RequirementsLangServices *>(&services);
}

[[nodiscard]] inline const TestsLangServices *
as_tests_lang_services(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const TestsLangServices *>(&services);
}

} // namespace requirements
