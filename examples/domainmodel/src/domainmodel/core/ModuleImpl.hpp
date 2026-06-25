#pragma once

#include <memory>

#include <domainmodel/core/Parser.hpp>
#include <domainmodel/core/Services.hpp>
#include <domainmodel/core/references/DomainModelScopeComputation.hpp>
#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>

namespace domainmodel::detail {

/// Applies the domain-model core overrides to any container that derives from
/// both `pegium::CoreServices` and `DomainModelAddedServices` — that is, either
/// `DomainModelCoreServices` (headless) or `lsp::DomainModelServices` (LSP).
///
/// Defined once in this header so the `core/` and `lsp/` translation units share
/// a single definition instead of each carrying its own copy.
template <typename Services>
void applyDomainModelCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::DomainModelParser>(services);
  services.languageMetaData.fileExtensions = {".dmodel"};
  services.qualifiedNameProvider =
      std::make_shared<const references::QualifiedNameProvider>();
  services.references.scopeComputation =
      std::make_unique<references::DomainModelScopeComputation>(services);
  services.validator = std::make_unique<validation::DomainModelValidator>();
  validation::registerValidationChecks(services);
}

} // namespace domainmodel::detail
