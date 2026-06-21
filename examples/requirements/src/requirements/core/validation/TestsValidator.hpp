#pragma once

#include <requirements/core/ast.hpp>

#include <pegium/core/validation/ValidationAcceptor.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace requirements::validation {

class TestsValidator final {
public:
  void checkTestNameContainsANumber(
      const requirements::ast::Test &test,
      const pegium::validation::ValidationAcceptor &accept) const;

  void checkTestReferencesOnlyEnvironmentsAlsoReferencedInSomeRequirement(
      const requirements::ast::Test &test,
      const pegium::validation::ValidationAcceptor &accept) const;
};

template <typename TServices>
void registerTestsValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &TestsValidator::checkTestNameContainsANumber>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &TestsValidator::
               checkTestReferencesOnlyEnvironmentsAlsoReferencedInSomeRequirement>(
           validator)});
}

} // namespace requirements::validation
