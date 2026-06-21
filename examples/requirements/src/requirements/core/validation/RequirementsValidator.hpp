#pragma once

#include <memory>
#include <unordered_set>

#include <requirements/core/ast.hpp>

#include <pegium/core/utils/Caching.hpp>
#include <pegium/core/validation/ValidationAcceptor.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace pegium::workspace {
class Documents;
}

namespace requirements::validation {

class RequirementsValidator final {
public:
  explicit RequirementsValidator(
      const pegium::SharedCoreServices &sharedServices);

  void checkRequirementNameContainsANumber(
      const requirements::ast::Requirement &requirement,
      const pegium::validation::ValidationAcceptor &accept) const;

  void checkRequirementIsCoveredByATest(
      const requirements::ast::Requirement &requirement,
      const pegium::validation::ValidationAcceptor &accept,
      const pegium::workspace::Documents &documents) const;

private:
  // Requirements referenced by at least one test. This set is identical for
  // every requirement, so it is computed once per workspace build and reused;
  // the check runs per requirement, and rescanning every document each time is
  // O(requirements × documents). Cleared automatically on any workspace update.
  // Held directly (not by shared_ptr): the registry binds this validator by
  // pointer, so it is never copied.
  using CoveredRequirements =
      std::unordered_set<const requirements::ast::Requirement *>;
  pegium::utils::WorkspaceCache<int, std::shared_ptr<const CoveredRequirements>>
      _coveredCache;
};

template <typename TServices>
void registerRequirementsValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.validator;
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
