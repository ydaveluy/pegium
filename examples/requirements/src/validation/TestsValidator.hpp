#pragma once

#include <requirements/ast.hpp>
#include <requirements/services/Services.hpp>

#include <pegium/core/validation/ValidationAcceptor.hpp>

namespace requirements::services::validation {

class TestsValidator final {
public:
  void checkTestNameContainsANumber(
      const requirements::ast::Test &test,
      const pegium::validation::ValidationAcceptor &accept) const;

  void checkTestReferencesOnlyEnvironmentsAlsoReferencedInSomeRequirement(
      const requirements::ast::Test &test,
      const pegium::validation::ValidationAcceptor &accept) const;
};

void registerTestsValidationChecks(
    requirements::services::TestsLangServices &services);

} // namespace requirements::services::validation
