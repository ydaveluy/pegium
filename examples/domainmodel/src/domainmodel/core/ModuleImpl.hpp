#pragma once

#include <memory>

#include <domainmodel/core/Services.hpp>
#include <domainmodel/core/references/DomainModelScopeComputation.hpp>
#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>

namespace domainmodel::parser {

// The language parser is declared here but defined in ModuleImpl.cpp, so the heavy
// grammar template instantiation happens in that single translation unit
// instead of in every TU that wires the module (the core and lsp modules).
std::unique_ptr<const pegium::parser::Parser>
makeDomainModelParser(const pegium::CoreServices &services);

} // namespace domainmodel::parser

namespace domainmodel::detail {

/// Applies the domain-model core overrides to any container that derives from
/// both `pegium::CoreServices` and `DomainModelAddedServices` — that is, either
/// `DomainModelCoreServices` (headless) or `lsp::DomainModelServices` (LSP).
///
/// Defined once in this header so the `core/` and `lsp/` translation units share
/// a single definition instead of each carrying its own copy. The parser is
/// built through `makeDomainModelParser`, so the grammar is instantiated only in
/// `ModuleImpl.cpp`, not in every TU that wires the module.
template <typename Services>
void applyDomainModelCoreModule(Services &services) {
  services.parser = parser::makeDomainModelParser(services);
  services.languageMetaData.fileExtensions = {".dmodel"};
  services.qualifiedNameProvider =
      std::make_shared<const references::QualifiedNameProvider>();
  services.references.scopeComputation =
      std::make_unique<references::DomainModelScopeComputation>(services);
  services.validator = std::make_unique<validation::DomainModelValidator>();
  validation::registerValidationChecks(services);
}

} // namespace domainmodel::detail
