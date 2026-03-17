#pragma once

#include <domainmodel/ast.hpp>

#include <pegium/services/Services.hpp>
#include <pegium/validation/ValidationRegistry.hpp>

namespace domainmodel::services::validation {

class DomainModelValidator final {
public:
  static void registerValidationChecks(
      pegium::validation::ValidationRegistry &registry,
      const pegium::services::Services &services);

private:
  void checkTypeNameStartsWithCapital(
      const domainmodel::ast::Type &type,
      const pegium::validation::ValidationAcceptor &accept) const;
};

} // namespace domainmodel::services::validation
