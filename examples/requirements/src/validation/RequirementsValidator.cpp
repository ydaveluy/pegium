#include "validation/RequirementsValidator.hpp"

#include <requirements/ast.hpp>

#include <algorithm>
#include <cctype>
#include <string_view>

#include <pegium/core/workspace/Documents.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>
#include <pegium/core/validation/DiagnosticRanges.hpp>

namespace requirements::services::validation {
namespace {

using namespace requirements::ast;

bool has_digit(std::string_view value) {
  return std::ranges::any_of(value, [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
}

} // namespace

void registerRequirementsValidationChecks(
    requirements::services::RequirementsLangServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.requirementsLang.validation.requirementsValidator;
  auto *documents = services.shared.workspace.documents.get();

  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
          &RequirementsValidator::checkRequirementNameContainsANumber>(
          validator)});
  registry.registerCheck<Requirement>(
      [validator = &validator, documents](
          const Requirement &requirement,
          const pegium::validation::ValidationAcceptor &accept) {
        validator->checkRequirementIsCoveredByATest(requirement, accept,
                                                    *documents);
      });
}

void RequirementsValidator::checkRequirementNameContainsANumber(
    const Requirement &requirement,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (!has_digit(requirement.name)) {
    accept.warning(requirement, "Requirement name " + requirement.name +
                                    " should contain a number.")
        .property<&Requirement::name>();
  }
}

void RequirementsValidator::checkRequirementIsCoveredByATest(
    const Requirement &requirement,
    const pegium::validation::ValidationAcceptor &accept,
    const pegium::workspace::Documents &documents) const {
  bool covered = false;
  for (const auto &document : documents.all()) {
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
                   "Requirement " + requirement.name + " not covered by a test.");
  }
}

} // namespace requirements::services::validation
