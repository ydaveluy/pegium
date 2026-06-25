#include <pegium/core/services/CoreServices.hpp>

#include <pegium/core/documentation/DefaultCommentProvider.hpp>
#include <pegium/core/documentation/DefaultDocumentationProvider.hpp>
#include <pegium/core/references/DefaultLinker.hpp>
#include <pegium/core/references/DefaultNameProvider.hpp>
#include <pegium/core/references/DefaultReferences.hpp>
#include <pegium/core/references/DefaultScopeComputation.hpp>
#include <pegium/core/references/DefaultScopeProvider.hpp>
#include <pegium/core/validation/DefaultDocumentValidator.hpp>
#include <pegium/core/validation/DefaultValidationRegistry.hpp>
#include <pegium/core/workspace/DefaultAstNodeDescriptionProvider.hpp>
#include <pegium/core/workspace/DefaultReferenceDescriptionProvider.hpp>

namespace pegium {

std::optional<std::string_view>
CoreServices::firstMissingService() const noexcept {
  // Single source of truth for the required-service set; keep in sync with
  // installDefaultCoreServices(...) below (which installs all of these except
  // `parser`, intentionally left to the caller). `isComplete()` derives from
  // this so the two can never disagree.
  if (!parser) {
    return "parser";
  }
  if (!documentation.commentProvider) {
    return "documentation.commentProvider";
  }
  if (!documentation.documentationProvider) {
    return "documentation.documentationProvider";
  }
  if (!references.nameProvider) {
    return "references.nameProvider";
  }
  if (!references.scopeProvider) {
    return "references.scopeProvider";
  }
  if (!references.references) {
    return "references.references";
  }
  if (!references.scopeComputation) {
    return "references.scopeComputation";
  }
  if (!references.linker) {
    return "references.linker";
  }
  if (!validation.validationRegistry) {
    return "validation.validationRegistry";
  }
  if (!validation.documentValidator) {
    return "validation.documentValidator";
  }
  if (!workspace.astNodeDescriptionProvider) {
    return "workspace.astNodeDescriptionProvider";
  }
  if (!workspace.referenceDescriptionProvider) {
    return "workspace.referenceDescriptionProvider";
  }
  return std::nullopt;
}

bool CoreServices::isComplete() const noexcept {
  return !firstMissingService();
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
        std::make_unique<validation::DefaultValidationRegistry>(services);
  }
  if (!services.validation.documentValidator) {
    services.validation.documentValidator =
        std::make_unique<validation::DefaultDocumentValidator>(services);
  }
}

} // namespace pegium
