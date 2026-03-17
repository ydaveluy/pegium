#include "validation/RequirementsValidator.hpp"

#include <requirements/ast.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

#include <pegium/services/SharedServices.hpp>
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

void RequirementsValidator::registerValidationChecks(
    pegium::validation::ValidationRegistry &registry,
    const pegium::services::Services &services) {
  const RequirementsValidator validator(services);
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
          &RequirementsValidator::checkRequirement>(validator)});
}

void RequirementsValidator::checkRequirement(
    const Requirement &requirement,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (!has_digit(requirement.name)) {
    accept.warning(requirement, "Requirement name " + requirement.name +
                                    " should contain a number.")
        .property<&Requirement::name>();
  }

  bool covered = false;
  for (const auto &document : _services->sharedServices.workspace.documents->all()) {
    const auto *model = pegium::ast_ptr_cast<TestModel>(document->parseResult.value);
    if (model == nullptr) {
      continue;
    }
    for (const auto &test : model->tests) {
      if (!test) {
        continue;
      }
      for (const auto &reference : test->requirements) {
        if (reference.get() == &requirement) {
          covered = true;
          break;
        }
      }
      if (covered) {
        break;
      }
    }
    if (covered) {
      break;
    }
  }

  if (!covered) {
    accept.warning(requirement,
                   "Requirement " + requirement.name + " not covered by a test.")
        .property<&Requirement::name>();
  }
}

} // namespace requirements::services::validation
