#pragma once

#include <memory>

#include <requirements/parser/Parser.hpp>

#include "validation/RequirementsValidator.hpp"
#include "validation/TestsValidator.hpp"

namespace requirements::detail {

template <typename TServices>
void configure_requirements_core_services(TServices &services) {
  services.parser =
      std::make_unique<const requirements::parser::RequirementsParser>(services);
  services.languageMetaData.fileExtensions = {".req"};
  services.requirementsLang.validation.requirementsValidator =
      std::make_unique<requirements::validation::RequirementsValidator>();
  requirements::validation::registerRequirementsValidationChecks(services);
}

template <typename TServices>
void configure_tests_core_services(TServices &services) {
  services.parser =
      std::make_unique<const requirements::parser::TestsParser>(services);
  services.languageMetaData.fileExtensions = {".tst"};
  services.testsLang.validation.testsValidator =
      std::make_unique<requirements::validation::TestsValidator>();
  requirements::validation::registerTestsValidationChecks(services);
}

} // namespace requirements::detail
