#pragma once

#include <domainmodel/ast.hpp>
#include <domainmodel/services/Services.hpp>
#include <pegium/core/validation/ValidationAcceptor.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace domainmodel::validation {

class DomainModelValidator final {
public:
  void checkEntityNameStartsWithCapital(
      const domainmodel::ast::Entity &entity,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkDataTypeNameStartsWithCapital(
      const domainmodel::ast::DataType &dataType,
      const pegium::validation::ValidationAcceptor &accept) const;
};

template <typename TServices>
void registerValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.domainModel.validation.domainModelValidator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkEntityNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkDataTypeNameStartsWithCapital>(validator)});
}

} // namespace domainmodel::validation
