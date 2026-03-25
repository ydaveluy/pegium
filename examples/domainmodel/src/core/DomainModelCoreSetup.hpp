#pragma once

#include <memory>

#include <domainmodel/parser/Parser.hpp>

#include "references/DomainModelScopeComputation.hpp"
#include "references/QualifiedNameProvider.hpp"
#include "validation/DomainModelValidator.hpp"

namespace domainmodel::detail {

template <typename TServices>
void configure_core_services(TServices &services) {
  services.parser =
      std::make_unique<const domainmodel::parser::DomainModelParser>(services);
  services.languageMetaData.fileExtensions = {".dmodel"};
  services.domainModel.references.qualifiedNameProvider =
      std::make_shared<const domainmodel::references::QualifiedNameProvider>();
  services.references.scopeComputation =
      std::make_unique<domainmodel::references::DomainModelScopeComputation>(services);
  services.domainModel.validation.domainModelValidator =
      std::make_unique<domainmodel::validation::DomainModelValidator>();
  domainmodel::validation::registerValidationChecks(services);
}

} // namespace domainmodel::detail
