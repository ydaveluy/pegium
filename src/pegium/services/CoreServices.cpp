#include <pegium/services/CoreServices.hpp>

#include <pegium/documentation/DefaultCommentProvider.hpp>
#include <pegium/documentation/DefaultDocumentationProvider.hpp>
#include <pegium/references/DefaultLinker.hpp>
#include <pegium/references/DefaultNameProvider.hpp>
#include <pegium/references/DefaultReferences.hpp>
#include <pegium/references/DefaultScopeComputation.hpp>
#include <pegium/references/DefaultScopeProvider.hpp>
#include <pegium/validation/DefaultDocumentValidator.hpp>
#include <pegium/validation/DefaultValidationRegistry.hpp>
#include <pegium/workspace/DefaultAstNodeDescriptionProvider.hpp>
#include <pegium/workspace/DefaultReferenceDescriptionProvider.hpp>

namespace pegium::services {

bool CoreServices::isComplete() const noexcept {
  return parser && references.nameProvider &&
         workspace.astNodeDescriptionProvider &&
         workspace.referenceDescriptionProvider &&
         references.scopeProvider && references.references &&
         references.scopeComputation && references.linker &&
         validation.validationRegistry &&
         validation.documentValidator && documentation.commentProvider &&
         documentation.documentationProvider;
}

void installDefaultCoreServices(CoreServices &services) {
  if (!services.documentation.commentProvider) {
    services.documentation.commentProvider =
        std::make_unique<documentation::DefaultCommentProvider>();
  }
  if (!services.documentation.documentationProvider) {
    services.documentation.documentationProvider =
        std::make_unique<documentation::DefaultDocumentationProvider>(services);
  }
  if (!services.references.nameProvider) {
    services.references.nameProvider =
        std::make_unique<references::DefaultNameProvider>();
  }
  if (!services.workspace.astNodeDescriptionProvider) {
    services.workspace.astNodeDescriptionProvider =
        std::make_unique<workspace::DefaultAstNodeDescriptionProvider>(services);
  }
  if (!services.workspace.referenceDescriptionProvider) {
    services.workspace.referenceDescriptionProvider =
        std::make_unique<workspace::DefaultReferenceDescriptionProvider>(
            services);
  }
  if (!services.references.scopeProvider) {
    services.references.scopeProvider =
        std::make_unique<references::DefaultScopeProvider>(services);
  }
  if (!services.references.references) {
    services.references.references =
        std::make_unique<references::DefaultReferences>(services);
  }
  if (!services.references.scopeComputation) {
    services.references.scopeComputation =
        std::make_unique<references::DefaultScopeComputation>(services);
  }
  if (!services.references.linker) {
    services.references.linker =
        std::make_unique<references::DefaultLinker>(services);
  }
  if (!services.validation.validationRegistry) {
    services.validation.validationRegistry =
        std::make_unique<validation::DefaultValidationRegistry>();
  }
  if (!services.validation.documentValidator) {
    services.validation.documentValidator =
        std::make_unique<validation::DefaultDocumentValidator>(services);
  }
}

} // namespace pegium::services
