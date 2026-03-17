#pragma once

#include <requirements/ast.hpp>

#include <pegium/services/Services.hpp>
#include <pegium/validation/ValidationRegistry.hpp>

namespace requirements::services::validation {

class TestsValidator final {
public:
  static void registerValidationChecks(
      pegium::validation::ValidationRegistry &registry,
      const pegium::services::Services &services);

private:
  void checkTest(const requirements::ast::Test &test,
                 const pegium::validation::ValidationAcceptor &accept) const;
};

} // namespace requirements::services::validation
