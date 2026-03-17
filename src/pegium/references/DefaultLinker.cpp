#include <pegium/references/DefaultLinker.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/services/CoreServices.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/workspace/AstDescriptions.hpp>

namespace pegium::references {

namespace {

TextOffset ref_offset(const AbstractReference &reference) {
  const auto refNode = reference.getRefNode();
  if (refNode.has_value()) {
    return refNode->getBegin();
  }
  return 0;
}

ReferenceResolution resolved_target(AstNode *target,
                                    const workspace::AstNodeDescription &description) {
  return ReferenceResolution{.node = target,
                             .description = &description,
                             .errorMessage = {}};
}

ReferenceResolution unresolved_target(std::string_view refText) {
  return ReferenceResolution{
      .node = nullptr,
      .errorMessage = "Could not resolve reference '" + std::string(refText) + "'.",
  };
}

std::optional<workspace::NodeKey>
make_node_key(const workspace::AstNodeDescription &description) {
  if (description.documentId == workspace::InvalidDocumentId ||
      description.symbolId == workspace::InvalidSymbolId) {
    return std::nullopt;
  }
  return workspace::NodeKey{.documentId = description.documentId,
                            .symbolId = description.symbolId};
}

} // namespace

void DefaultLinker::link(workspace::Document &document,
                         const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);

  document.referenceDescriptions.clear();

  if (!document.hasAst() || document.parseResult.value == nullptr) {
    return;
  }

  for (const auto &handle : document.references) {
    auto *reference = handle.get();
    if (reference == nullptr) {
      continue;
    }
    if (reference->isMulti()) {
      reference->setResolution(resolveAll(*reference));
      continue;
    }
    reference->setResolution(resolve(*reference));
  }

  utils::throw_if_cancelled(cancelToken);
}

ReferenceResolution
DefaultLinker::resolve(const AbstractReference &reference) const {
  reference.markResolving();
  const auto info = makeReferenceInfo(reference);
  const auto *scopeProvider = coreServices.references.scopeProvider.get();
  if (reference.hasError()) {
    return ReferenceResolution{
        .node = nullptr,
        .description = nullptr,
        .errorMessage = reference.getErrorMessage(),
    };
  }
  if (scopeProvider == nullptr) {
    return unresolved_target(reference.getRefText());
  }

  const auto *candidate = scopeProvider->getScopeEntry(info);
  if (reference.hasError()) {
    return ReferenceResolution{
        .node = nullptr,
        .description = nullptr,
        .errorMessage = reference.getErrorMessage(),
    };
  }
  if (candidate == nullptr) {
    return unresolved_target(reference.getRefText());
  }

  auto *target = candidate->node != nullptr
                     ? const_cast<AstNode *>(candidate->node)
                     : loadAstNode(*candidate);
  if (target == nullptr) {
    return unresolved_target(reference.getRefText());
  }

  return resolved_target(target, *candidate);
}

MultiReferenceResolution
DefaultLinker::resolveAll(const AbstractReference &reference) const {
  reference.markResolving();
  const auto info = makeReferenceInfo(reference);
  const auto *scopeProvider = coreServices.references.scopeProvider.get();
  if (reference.hasError()) {
    return MultiReferenceResolution{
        .items = {},
        .errorMessage = reference.getErrorMessage(),
    };
  }

  MultiReferenceResolution resolution;
  if (scopeProvider != nullptr) {
    std::unordered_set<workspace::NodeKey, workspace::NodeKeyHash> seen;
    for (const auto *candidate : scopeProvider->getScopeEntries(info)) {
      if (candidate == nullptr) {
        continue;
      }
      const auto key = make_node_key(*candidate);
      if (key.has_value() && !seen.insert(*key).second) {
        continue;
      }
      auto *target = candidate->node != nullptr
                         ? const_cast<AstNode *>(candidate->node)
                         : loadAstNode(*candidate);
      if (target == nullptr) {
        continue;
      }
      resolution.items.push_back(resolved_target(target, *candidate));
    }
  }
  if (reference.hasError()) {
    return MultiReferenceResolution{
        .items = {},
        .errorMessage = reference.getErrorMessage(),
    };
  }
  if (resolution.items.empty()) {
    resolution.errorMessage =
        unresolved_target(reference.getRefText()).errorMessage;
  }
  return resolution;
}

void DefaultLinker::unlink(workspace::Document &document,
                           const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  for (const auto &handle : document.references) {
    auto *reference = handle.get();
    if (reference != nullptr) {
      reference->clearLinkState();
    }
  }
  document.referenceDescriptions.clear();
  utils::throw_if_cancelled(cancelToken);
}

workspace::AstNodeDescriptionOrError
DefaultLinker::getCandidate(const ReferenceInfo &reference) const {
  if (reference.reference == nullptr) {
    return createLinkingError(reference,
                              "Could not resolve reference: missing context.");
  }
  const auto *scopeProvider = coreServices.references.scopeProvider.get();
  if (scopeProvider == nullptr) {
    return createLinkingError(
        reference, "Could not resolve reference named '" +
                       std::string(reference.reference->getRefText()) + "'.");
  }
  const auto *candidate = scopeProvider->getScopeEntry(reference);
  if (reference.reference->hasError()) {
    return createLinkingError(reference, reference.reference->getErrorMessage());
  }
  if (candidate == nullptr) {
    return createLinkingError(
        reference, "Could not resolve reference named '" +
                       std::string(reference.reference->getRefText()) + "'.");
  }

  return *candidate;
}

workspace::AstNodeDescriptionsOrError
DefaultLinker::getCandidates(const ReferenceInfo &reference) const {
  if (reference.reference == nullptr) {
    return createLinkingError(reference,
                              "Could not resolve reference: missing context.");
  }
  const auto *scopeProvider = coreServices.references.scopeProvider.get();
  if (scopeProvider == nullptr) {
    return createLinkingError(
        reference, "Could not resolve reference named '" +
                       std::string(reference.reference->getRefText()) + "'.");
  }

  std::vector<workspace::AstNodeDescription> descriptions;
  std::unordered_set<workspace::NodeKey, workspace::NodeKeyHash> seen;
  for (const auto *candidate : scopeProvider->getScopeEntries(reference)) {
    if (candidate == nullptr) {
      continue;
    }
    const auto key = make_node_key(*candidate);
    if (key.has_value() && !seen.insert(*key).second) {
      continue;
    }
    descriptions.push_back(*candidate);
  }
  if (reference.reference->hasError()) {
    return createLinkingError(reference, reference.reference->getErrorMessage());
  }

  if (descriptions.empty()) {
    return createLinkingError(
        reference, "Could not resolve reference named '" +
                       std::string(reference.reference->getRefText()) + "'.");
  }
  return descriptions;
}

AstNode *DefaultLinker::loadAstNode(
    const workspace::AstNodeDescription &description) const {
  if (description.node != nullptr) {
    return const_cast<AstNode *>(description.node);
  }
  if (description.documentId == workspace::InvalidDocumentId) {
    return nullptr;
  }

  const auto *documents = coreServices.shared.workspace.documents.get();
  if (documents == nullptr) {
    return nullptr;
  }
  const auto document = documents->getDocument(description.documentId);
  if (!document || !document->hasAst() ||
      document->parseResult.value == nullptr) {
    return nullptr;
  }

  if (const auto *node = document->findAstNode(description.symbolId);
      node != nullptr) {
    return const_cast<AstNode *>(node);
  }
  return nullptr;
}

workspace::LinkingError DefaultLinker::createLinkingError(
    ReferenceInfo reference, std::string message,
    std::optional<workspace::AstNodeDescription> targetDescription) const {
  return workspace::LinkingError{.info = reference,
                                 .message = std::move(message),
                                 .targetDescription =
                                     std::move(targetDescription)};
}

} // namespace pegium::references
