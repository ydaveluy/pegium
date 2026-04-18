#pragma once

#include <memory>
#include <string>
#include <utility>

#include <domainmodel/core/Services.hpp>
#include <domainmodel/core/references/DomainModelScopeComputation.hpp>
#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>
#include <domainmodel/parser/Parser.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace domainmodel {

/// Core service overrides applied on top of pegium's default core services.
template <typename Services>
void installDomainModelCoreModule(Services &services) {
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

/// Builds the core-only domain-model language services.
inline std::unique_ptr<DomainModelCoreServices>
createDomainModelServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId = "domain-model") {
  auto services = std::make_unique<DomainModelCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installDomainModelCoreModule(*services);
  return services;
}

/// Registers the core-only domain-model services in `sharedServices`.
inline bool
registerDomainModelServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelServices(sharedServices));
  return true;
}

} // namespace domainmodel
