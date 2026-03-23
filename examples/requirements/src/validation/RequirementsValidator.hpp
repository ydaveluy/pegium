#pragma once

#include <requirements/ast.hpp>
#include <requirements/services/Services.hpp>

#include <pegium/core/validation/ValidationAcceptor.hpp>

namespace pegium::workspace {
class Documents;
}

namespace requirements::services::validation {

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

void registerRequirementsValidationChecks(
    requirements::services::RequirementsLangServices &services);

} // namespace requirements::services::validation
