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

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace domainmodel::services {

DomainModelServices::DomainModelServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

DomainModelServices::DomainModelServices(DomainModelServices &&) noexcept = default;

DomainModelServices::~DomainModelServices() noexcept = default;

std::unique_ptr<DomainModelServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId) {
  auto services = pegium::services::makeDefaultServices<DomainModelServices>(
      sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const domainmodel::parser::DomainModelParser>(*services);
  services->languageMetaData.fileExtensions = {".dmodel"};

  services->domainModel.references.qualifiedNameProvider =
      std::make_shared<const references::QualifiedNameProvider>();
  services->references.scopeComputation =
      std::make_unique<references::DomainModelScopeComputation>(*services);
  services->domainModel.validation.domainModelValidator =
      std::make_unique<validation::DomainModelValidator>();
  services->lsp.renameProvider =
      std::make_unique<lsp::DomainModelRenameProvider>(*services);
  services->lsp.formatter =
      std::make_unique<lsp::DomainModelFormatter>(*services);

  validation::registerValidationChecks(*services);
  return services;
}

bool register_language_services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "domain-model"));
  return true;
}

} // namespace domainmodel::services
