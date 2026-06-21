#include <domainmodel/core/Module.hpp>

#include <utility>

#include <domainmodel/core/references/DomainModelScopeComputation.hpp>
#include <domainmodel/core/references/QualifiedNameProvider.hpp>
#include <domainmodel/core/validation/DomainModelValidator.hpp>
#include <domainmodel/core/Parser.hpp>

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

void installDomainModelCoreModule(DomainModelCoreServices &services) {
  applyDomainModelCoreModule(services);
}

std::unique_ptr<DomainModelCoreServices>
createDomainModelServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId) {
  auto services = std::make_unique<DomainModelCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installDomainModelCoreModule(*services);
  return services;
}

bool registerDomainModelServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelServices(sharedServices));
  return true;
}

} // namespace domainmodel
