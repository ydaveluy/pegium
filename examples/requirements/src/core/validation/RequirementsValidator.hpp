#pragma once

#include <requirements/ast.hpp>
#include <requirements/services/Services.hpp>

#include <pegium/core/validation/ValidationAcceptor.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace pegium::workspace {
class Documents;
}

namespace requirements::validation {

class RequirementsValidator final {
public:
  void checkRequirementNameContainsANumber(
      const requirements::ast::Requirement &requirement,
      const pegium::validation::ValidationAcceptor &accept) const;

  void checkRequirementIsCoveredByATest(
      const requirements::ast::Requirement &requirement,
      const pegium::validation::ValidationAcceptor &accept,
      const pegium::workspace::Documents &documents) const;
};

template <typename TServices>
void registerRequirementsValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.requirementsLang.validation.requirementsValidator;
  auto *documents = services.shared.workspace.documents.get();

  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
          &RequirementsValidator::checkRequirementNameContainsANumber>(
          validator)});
  registry.template registerCheck<requirements::ast::Requirement>(
      [validator = &validator, documents](
          const requirements::ast::Requirement &requirement,
          const pegium::validation::ValidationAcceptor &accept) {
        validator->checkRequirementIsCoveredByATest(requirement, accept,
                                                    *documents);
      });
}

} // namespace requirements::validation
