#include "validation/TestsValidator.hpp"

#include <requirements/ast.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

#include <pegium/validation/DiagnosticRanges.hpp>

namespace requirements::services::validation {
namespace {

using namespace requirements::ast;

bool has_digit(std::string_view value) {
  return std::ranges::any_of(value, [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
}

} // namespace

void TestsValidator::registerValidationChecks(
    pegium::validation::ValidationRegistry &registry,
    const pegium::services::Services & /*services*/) {
  const TestsValidator validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
          &TestsValidator::checkTest>(validator)});
}

void TestsValidator::checkTest(
    const Test &test,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (!has_digit(test.name)) {
    accept.warning(test, "Test name " + test.name + " should contain a number.")
        .property<&Test::name>();
  }

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
