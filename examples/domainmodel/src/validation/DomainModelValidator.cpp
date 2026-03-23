#include "validation/DomainModelValidator.hpp"

#include <domainmodel/ast.hpp>

#include <cctype>

namespace domainmodel::services::validation {
using namespace domainmodel::ast;

void registerValidationChecks(
    domainmodel::services::DomainModelServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.domainModel.validation.domainModelValidator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkEntityNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &DomainModelValidator::checkDataTypeNameStartsWithCapital>(validator)});
}

void DomainModelValidator::checkEntityNameStartsWithCapital(
    const Entity &entity,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (entity.name.empty()) {
    return;
  }
  const auto first = entity.name.front();
  if (std::toupper(static_cast<unsigned char>(first)) == first) {
    return;
  }
  accept.warning(entity, "Type name should start with a capital.")
      .property<&Entity::name>();
}

void DomainModelValidator::checkDataTypeNameStartsWithCapital(
    const DataType &dataType,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (dataType.name.empty()) {
    return;
  }
  const auto first = dataType.name.front();
  if (std::toupper(static_cast<unsigned char>(first)) == first) {
    return;
  }
  accept.warning(dataType, "Type name should start with a capital.")
      .property<&DataType::name>();
}

} // namespace domainmodel::services::validation
