#include "validation/DomainModelValidator.hpp"

#include <domainmodel/ast.hpp>

#include <cctype>
#include <optional>

#include <pegium/validation/DiagnosticRanges.hpp>

namespace domainmodel::services::validation {
namespace {

using namespace domainmodel::ast;

std::string type_name(const Type &type) {
  if (const auto *entity = dynamic_cast<const Entity *>(&type)) {
    return entity->name;
  }
  if (const auto *dataType = dynamic_cast<const DataType *>(&type)) {
    return dataType->name;
  }
  return {};
}

} // namespace

void DomainModelValidator::registerValidationChecks(
    pegium::validation::ValidationRegistry &registry,
    const pegium::services::Services & /*services*/) {
  const DomainModelValidator validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
          &DomainModelValidator::checkTypeNameStartsWithCapital>(validator)});
}

void DomainModelValidator::checkTypeNameStartsWithCapital(
    const Type &type, const pegium::validation::ValidationAcceptor &accept) const {
  const auto name = type_name(type);
  if (name.empty()) {
    return;
  }
  const auto first = name.front();
  if (std::toupper(static_cast<unsigned char>(first)) == first) {
    return;
  }
  if (const auto *entity = dynamic_cast<const Entity *>(&type)) {
    accept.warning(*entity, "Type name should start with a capital.")
        .property<&Entity::name>();
    return;
  }
  if (const auto *dataType = dynamic_cast<const DataType *>(&type)) {
    accept.warning(*dataType, "Type name should start with a capital.")
        .property<&DataType::name>();
  }
}

} // namespace domainmodel::services::validation
