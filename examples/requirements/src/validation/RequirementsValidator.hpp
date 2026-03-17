#pragma once

#include <requirements/ast.hpp>

#include <pegium/services/Services.hpp>
#include <pegium/validation/ValidationRegistry.hpp>

namespace requirements::services::validation {

class RequirementsValidator final {
public:
  static void registerValidationChecks(
      pegium::validation::ValidationRegistry &registry,
      const pegium::services::Services &services);

private:
  explicit RequirementsValidator(
      const pegium::services::Services &services) noexcept
      : _services(&services) {}

  void checkRequirement(const requirements::ast::Requirement &requirement,
                        const pegium::validation::ValidationAcceptor &accept) const;

  const pegium::services::Services *_services;
};

} // namespace requirements::services::validation
