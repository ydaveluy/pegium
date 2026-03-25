#pragma once

#include <requirements/services/Services.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace requirements::lsp {

struct RequirementsLangServices final : pegium::Services {
  explicit RequirementsLangServices(
      const pegium::SharedServices &sharedServices);
  RequirementsLangServices(RequirementsLangServices &&) noexcept;
  RequirementsLangServices &operator=(RequirementsLangServices &&) noexcept =
      delete;
  RequirementsLangServices(const RequirementsLangServices &) = delete;
  RequirementsLangServices &operator=(const RequirementsLangServices &) = delete;
  ~RequirementsLangServices() noexcept override;

  requirements::RequirementsLangAddedServices requirementsLang;
};

struct TestsLangServices final : pegium::Services {
  explicit TestsLangServices(
      const pegium::SharedServices &sharedServices);
  TestsLangServices(TestsLangServices &&) noexcept;
  TestsLangServices &operator=(TestsLangServices &&) noexcept = delete;
  TestsLangServices(const TestsLangServices &) = delete;
  TestsLangServices &operator=(const TestsLangServices &) = delete;
  ~TestsLangServices() noexcept override;

  requirements::TestsLangAddedServices testsLang;
};

[[nodiscard]] inline const RequirementsLangServices *
as_requirements_lang_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const RequirementsLangServices *>(&services);
}

[[nodiscard]] inline const TestsLangServices *
as_tests_lang_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const TestsLangServices *>(&services);
}

} // namespace requirements::lsp
