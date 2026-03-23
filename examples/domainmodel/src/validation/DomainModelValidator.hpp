#pragma once

#include <domainmodel/ast.hpp>
#include <domainmodel/services/Services.hpp>
#include <pegium/core/validation/ValidationAcceptor.hpp>

namespace domainmodel::services::validation {

class DomainModelValidator final {
public:
  void checkEntityNameStartsWithCapital(
      const domainmodel::ast::Entity &entity,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkDataTypeNameStartsWithCapital(
      const domainmodel::ast::DataType &dataType,
      const pegium::validation::ValidationAcceptor &accept) const;
};

void registerValidationChecks(
    domainmodel::services::DomainModelServices &services);

} // namespace domainmodel::services::validation
