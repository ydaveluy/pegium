#include <pegium/core/workspace/DefaultReferenceDescriptionProvider.hpp>

#include <algorithm>
#include <cassert>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::workspace {

std::vector<ReferenceDescription>
DefaultReferenceDescriptionProvider::createDescriptions(
    const Document &document, const utils::CancellationToken &cancelToken) const {
  std::vector<ReferenceDescription> descriptions;
  descriptions.reserve(document.references.size());

  for (const auto &handle : document.references) {
    utils::throw_if_cancelled(cancelToken);
    const auto &reference = *handle.getConst();
    if (!reference.isResolved()) {
      continue;
    }

    const auto targetText = reference.getRefText();
    if (targetText.empty()) {
      continue;
    }

    TextOffset sourceOffset = 0;
    TextOffset sourceLength = static_cast<TextOffset>(targetText.size());
    if (const auto refNode = reference.getRefNode(); refNode.has_value()) {
      sourceOffset = refNode->getBegin();
      sourceLength = refNode->getEnd() - refNode->getBegin();
    }
    ReferenceDescription description{
        .sourceDocumentId = document.id,
        .sourceOffset = sourceOffset,
        .sourceLength = sourceLength,
        .referenceType = reference.getReferenceType(),
    };

    if (reference.isMultiReference()) {
      const auto &multi =
          static_cast<const AbstractMultiReference &>(reference);
      for (std::size_t index = 0; index < multi.resolvedDescriptionCount();
           ++index) {
        auto resolvedDescription = description;
        const auto &target = multi.resolvedDescriptionAt(index);
        resolvedDescription.targetDocumentId = target.documentId;
        resolvedDescription.local =
            resolvedDescription.sourceDocumentId == target.documentId;
        resolvedDescription.targetSymbolId = target.symbolId;
        descriptions.push_back(std::move(resolvedDescription));
      }
      continue;
    }

    const auto &single = static_cast<const AbstractSingleReference &>(reference);
    const auto &target = single.resolvedDescription();
    description.targetDocumentId = target.documentId;
    description.local = description.sourceDocumentId == target.documentId;
    description.targetSymbolId = target.symbolId;
    descriptions.push_back(std::move(description));
  }

  return descriptions;
}

} // namespace pegium::workspace
