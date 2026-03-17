#include <domainmodel/services/Module.hpp>

#include <domainmodel/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/DomainModelFormatter.hpp"
#include "lsp/DomainModelRenameProvider.hpp"
#include "references/DomainModelScopeComputation.hpp"
#include "references/QualifiedNameProvider.hpp"
#include "validation/DomainModelValidator.hpp"

namespace domainmodel::services {

std::unique_ptr<pegium::services::Services>
create_language_services(const pegium::services::SharedServices &sharedServices,
                         std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const domainmodel::parser::DomainModelParser>(*services);
  services->languageMetaData.fileExtensions = {".dmodel"};

  auto qualifiedNameProvider =
      std::make_shared<const references::QualifiedNameProvider>();
  services->references.scopeComputation =
      std::make_unique<references::DomainModelScopeComputation>(
          *services, qualifiedNameProvider);
  services->lsp.renameProvider = std::make_unique<lsp::DomainModelRenameProvider>(
      *services, *sharedServices.workspace.indexManager,
      *sharedServices.workspace.documents, qualifiedNameProvider);
  services->lsp.formatter =
      std::make_unique<lsp::DomainModelFormatter>(*services);

  validation::DomainModelValidator::registerValidationChecks(
      *services->validation.validationRegistry, *services);
  return services;
}

bool register_language_services(pegium::services::SharedServices &sharedServices) {
  return sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "domain-model"));
}

} // namespace domainmodel::services
