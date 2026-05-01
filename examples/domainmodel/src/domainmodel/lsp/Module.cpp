#include <domainmodel/lsp/Module.hpp>

#include <utility>

#include <domainmodel/core/references/DomainModelScopeComputation.hpp>
#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>
#include <domainmodel/lsp/DomainModelFormatter.hpp>
#include <domainmodel/lsp/DomainModelRenameProvider.hpp>
#include <domainmodel/parser/Parser.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace domainmodel {

namespace {
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
} // namespace

void installDomainModelCoreModule(lsp::DomainModelServices &services) {
  applyDomainModelCoreModule(services);
}

} // namespace domainmodel

namespace domainmodel::lsp {

void installDomainModelLspModule(DomainModelServices &services) {
  services.lsp.renameProvider =
      std::make_unique<DomainModelRenameProvider>(services);
  services.lsp.formatter = std::make_unique<DomainModelFormatter>(services);
}

std::unique_ptr<DomainModelServices>
createDomainModelServices(const pegium::SharedServices &sharedServices,
                          std::string languageId) {
  auto services = pegium::makeDefaultServices<DomainModelServices>(
      sharedServices, std::move(languageId));
  domainmodel::installDomainModelCoreModule(*services);
  installDomainModelLspModule(*services);
  return services;
}

bool registerDomainModelServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelServices(sharedServices));
  return true;
}

} // namespace domainmodel::lsp
