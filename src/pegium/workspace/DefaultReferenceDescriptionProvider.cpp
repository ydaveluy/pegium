#include <pegium/workspace/DefaultReferenceDescriptionProvider.hpp>

#include <algorithm>

#include <pegium/services/CoreServices.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::workspace {

namespace {

void apply_resolved_target(ReferenceDescription &description,
                           const AstNodeDescription *target) {
  if (target != nullptr && target->documentId != InvalidDocumentId) {
    description.targetDocumentId = target->documentId;
  }
  if (target != nullptr && target->symbolId != InvalidSymbolId) {
    description.targetSymbolId = target->symbolId;
  }
}

} // namespace

std::vector<ReferenceDescription>
DefaultReferenceDescriptionProvider::createDescriptions(
    const Document &document, const utils::CancellationToken &cancelToken) const {
  std::vector<ReferenceDescription> descriptions;
  descriptions.reserve(document.references.size());

  for (const auto &handle : document.references) {
    utils::throw_if_cancelled(cancelToken);
    const auto *reference = handle.getConst();
    if (reference == nullptr) {
      continue;
    }

    if (!reference->isResolved()) {
      continue;
    }

    const auto targetText = reference->getRefText();
    if (targetText.empty()) {
      continue;
    }

    const auto refNode = reference->getRefNode();
    ReferenceDescription description{
        .sourceDocumentId = document.id,
        .sourceOffset = refNode.has_value() ? refNode->getBegin() : 0,
        .sourceLength = refNode.has_value()
                            ? refNode->getEnd() - refNode->getBegin()
                            : static_cast<TextOffset>(targetText.size()),
        .referenceType = reference->getReferenceType(),
    };

    bool emitted = false;
    reference->forEachResolvedTargetDescription(
        [&](const AstNodeDescription *target) {
          emitted = true;
          auto resolvedDescription = description;
          apply_resolved_target(resolvedDescription, target);
          descriptions.push_back(std::move(resolvedDescription));
        });
    if (!emitted) {
      continue;
    }
  }

  return descriptions;
}

} // namespace pegium::workspace
