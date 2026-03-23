#include "validation/TestsValidator.hpp"

#include <requirements/ast.hpp>

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

#include <pegium/core/validation/DiagnosticRanges.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace requirements::services::validation {
namespace {

using namespace requirements::ast;

bool has_digit(std::string_view value) {
  return std::ranges::any_of(value, [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
}

} // namespace

void registerTestsValidationChecks(
    requirements::services::TestsLangServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.testsLang.validation.testsValidator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &TestsValidator::checkTestNameContainsANumber>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &TestsValidator::
               checkTestReferencesOnlyEnvironmentsAlsoReferencedInSomeRequirement>(
           validator)});
}

void TestsValidator::checkTestNameContainsANumber(
    const Test &test,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (!has_digit(test.name)) {
    accept.warning(test, "Test name " + test.name + " should contain a number.")
        .property<&Test::name>();
  }
}

void TestsValidator::
    checkTestReferencesOnlyEnvironmentsAlsoReferencedInSomeRequirement(
        const Test &test,
        const pegium::validation::ValidationAcceptor &accept) const {
  for (std::size_t environmentIndex = 0;
       environmentIndex < test.environments.size(); ++environmentIndex) {
    const auto &environment = test.environments[environmentIndex];
    const auto *resolvedEnvironment = environment.get();
    if (resolvedEnvironment == nullptr) {
      continue;
    }

    bool valid = false;
    for (const auto &requirementRef : test.requirements) {
      const auto *resolvedRequirement = requirementRef.get();
      if (resolvedRequirement == nullptr) {
        continue;
      }
      for (const auto &requiredEnvironment : resolvedRequirement->environments) {
        if (requiredEnvironment.get() == resolvedEnvironment) {
          valid = true;
          break;
        }
      }
      if (valid) {
        break;
      }
    }

    if (!valid) {
      accept.warning(
          test, "Test " + test.name + " references environment " +
                    resolvedEnvironment->name +
                    " which is not used by any referenced requirement.")
          .property<&Test::environments>(environmentIndex);
    }
  }
}

} // namespace requirements::services::validation
