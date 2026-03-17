#include "lsp/DomainModelRenameProvider.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace domainmodel::services::lsp {
namespace {

std::string last_segment(std::string_view name) {
  const auto pos = name.rfind('.');
  if (pos == std::string_view::npos) {
    return std::string(name);
  }
  return std::string(name.substr(pos + 1));
}

std::string reference_text(const pegium::workspace::Documents *documentStore,
                           const pegium::workspace::ReferenceDescription &reference) {
  if (documentStore == nullptr) {
    return {};
  }
  const auto document = documentStore->getDocument(reference.sourceDocumentId);
  if (document == nullptr) {
    return {};
  }
  return std::string(reference.sourceText(document->textView()));
}

std::string edit_key(std::string_view uri, pegium::TextOffset begin,
                     pegium::TextOffset end) {
  return std::format("{}:{}:{}", uri, begin, end);
}

std::optional<pegium::workspace::AstNodeDescription>
find_target_symbol(const pegium::workspace::IndexManager &indexManager,
                   pegium::workspace::DocumentId documentId,
                   pegium::TextOffset offset) {
  for (const auto &symbol : indexManager.elementsForDocument(documentId)) {
    const auto end =
        static_cast<pegium::TextOffset>(symbol.offset + symbol.nameLength);
    if (symbol.nameLength != 0 && offset >= symbol.offset && offset <= end) {
      return symbol;
    }
  }
  return std::nullopt;
}

bool matches_target(const pegium::workspace::Documents *documentStore,
                    const pegium::workspace::ReferenceDescription &reference,
                    const pegium::workspace::AstNodeDescription &target,
                    std::string_view oldSimpleName,
                    std::string_view oldQualifiedName) {
  const auto refText = reference_text(documentStore, reference);
  if (reference.isResolved()) {
    if (!reference.targetDocumentId.has_value() ||
        *reference.targetDocumentId != target.documentId) {
      return false;
    }
    if (reference.targetSymbolId.has_value() &&
        *reference.targetSymbolId == target.symbolId) {
      return true;
    }
    return refText == oldSimpleName || refText == oldQualifiedName;
  }
  return refText == oldSimpleName || refText == oldQualifiedName;
}

} // namespace

DomainModelRenameProvider::DomainModelRenameProvider(
    const pegium::services::Services &services,
    const pegium::workspace::IndexManager &indexManager,
    const pegium::workspace::Documents &documentStore,
    std::shared_ptr<const references::QualifiedNameProvider>
        qualifiedNameProvider)
    : pegium::lsp::DefaultRenameProvider(services),
      _indexManager(&indexManager), _documentStore(&documentStore),
      _qualifiedNameProvider(std::move(qualifiedNameProvider)) {}

std::optional<::lsp::WorkspaceEdit> DomainModelRenameProvider::rename(
    const pegium::workspace::Document &document, const ::lsp::RenameParams &params,
    const pegium::utils::CancellationToken &cancelToken) const {
  const auto newName = std::string_view(params.newName);
  auto baseEdit =
      pegium::lsp::DefaultRenameProvider::rename(document, params, cancelToken);
  if (_indexManager == nullptr || _documentStore == nullptr || newName.empty()) {
    return baseEdit;
  }

  const auto offset = document.positionToOffset(params.position);
  const auto target = find_target_symbol(*_indexManager, document.id, offset);
  if (!target.has_value()) {
    return baseEdit;
  }

  const auto oldQualifiedName = target->name;
  const auto oldSimpleName = last_segment(target->name);
  std::string qualifier;
  if (oldQualifiedName.size() > oldSimpleName.size()) {
    qualifier = std::string(oldQualifiedName.substr(
        0, oldQualifiedName.size() - oldSimpleName.size() - 1));
  }
  const auto newQualifiedName =
      _qualifiedNameProvider != nullptr
          ? _qualifiedNameProvider->getQualifiedName(qualifier, newName)
          : qualifier.empty() ? std::string(newName)
                              : qualifier + "." + std::string(newName);
  const auto document_for_id =
      [&](pegium::workspace::DocumentId documentId)
          -> const pegium::workspace::Document * {
    if (documentId == document.id) {
      return &document;
    }
    if (_documentStore == nullptr) {
      return nullptr;
    }
    const auto targetDocument = _documentStore->getDocument(documentId);
    return targetDocument != nullptr ? targetDocument.get() : nullptr;
  };

  if (!baseEdit.has_value()) {
    ::lsp::Map<::lsp::DocumentUri, ::lsp::Array<::lsp::TextEdit>> changes;
    std::unordered_set<std::string> seenEdits;
    auto add_edit = [&](pegium::workspace::DocumentId documentId,
                        pegium::TextOffset begin, pegium::TextOffset end,
                        std::string text) {
      const auto *targetDocument = document_for_id(documentId);
      if (targetDocument == nullptr) {
        return;
      }
      const auto key = edit_key(targetDocument->uri, begin, end);
      if (!seenEdits.insert(key).second) {
        return;
      }

      ::lsp::TextEdit edit{};
      edit.range.start = targetDocument->offsetToPosition(begin);
      edit.range.end = targetDocument->offsetToPosition(end);
      edit.newText = std::move(text);
      changes[::lsp::Uri::parse(targetDocument->uri)].push_back(std::move(edit));
    };

    add_edit(target->documentId, target->offset,
             static_cast<pegium::TextOffset>(target->offset + oldSimpleName.size()),
             std::string(newName));

    for (const auto &indexedDocument : _documentStore->all()) {
      if (indexedDocument == nullptr) {
        continue;
      }
      for (const auto &reference :
           _indexManager->referenceDescriptionsForDocument(indexedDocument->id)) {
        if (!matches_target(_documentStore, reference, *target, oldSimpleName,
                            oldQualifiedName)) {
          continue;
        }
        const auto replacementLength = reference.sourceLength;
        const auto refText = reference_text(_documentStore, reference);
        const bool qualified =
            isQualifiedReference(reference) || refText == oldQualifiedName ||
            replacementLength >
                static_cast<pegium::TextOffset>(oldSimpleName.size());
        add_edit(reference.sourceDocumentId, reference.sourceOffset,
                 static_cast<pegium::TextOffset>(reference.sourceOffset +
                                                replacementLength),
                 qualified ? newQualifiedName : std::string(newName));
      }
    }

    for (auto &[uri, edits] : changes) {
      const auto *targetDocument =
          _documentStore != nullptr ? _documentStore->getDocument(uri.toString()).get()
                                    : nullptr;
      if (targetDocument == nullptr) {
        continue;
      }

      std::ranges::sort(edits, [&](const ::lsp::TextEdit &left,
                                   const ::lsp::TextEdit &right) {
        return targetDocument->positionToOffset(left.range.start) >
               targetDocument->positionToOffset(right.range.start);
      });
    }
    if (changes.empty()) {
      return std::nullopt;
    }
    ::lsp::WorkspaceEdit fallbackEdit{};
    fallbackEdit.changes = std::move(changes);
    return fallbackEdit;
  }

  if (newQualifiedName == params.newName || !baseEdit->changes.has_value()) {
    return baseEdit;
  }

  std::unordered_set<std::string> qualifiedEditKeys;
  for (const auto &indexedDocument : _documentStore->all()) {
    if (indexedDocument == nullptr) {
      continue;
    }
    for (const auto &reference :
         _indexManager->referenceDescriptionsForDocument(indexedDocument->id)) {
      if (!matches_target(_documentStore, reference, *target, oldSimpleName,
                          oldQualifiedName)) {
        continue;
      }
      const auto replacementLength = reference.sourceLength;
      const auto refText = reference_text(_documentStore, reference);
      const bool qualified =
          isQualifiedReference(reference) || refText == oldQualifiedName ||
          replacementLength >
              static_cast<pegium::TextOffset>(oldSimpleName.size());
      if (!qualified) {
        continue;
      }
      qualifiedEditKeys.insert(edit_key(
          indexedDocument->uri, reference.sourceOffset,
          static_cast<pegium::TextOffset>(reference.sourceOffset +
                                          replacementLength)));
    }
  }

  for (auto &[uri, edits] : *baseEdit->changes) {
    const auto *targetDocument =
        _documentStore != nullptr ? _documentStore->getDocument(uri.toString()).get()
                                  : nullptr;
    if (targetDocument == nullptr) {
      continue;
    }
    for (auto &edit : edits) {
      const auto key = edit_key(
          uri.toString(), targetDocument->positionToOffset(edit.range.start),
          targetDocument->positionToOffset(edit.range.end));
      if (qualifiedEditKeys.contains(key)) {
        edit.newText = newQualifiedName;
      }
    }
  }

  return baseEdit;
}

bool DomainModelRenameProvider::isQualifiedReference(
    const pegium::workspace::ReferenceDescription &reference) const {
  if (_documentStore == nullptr || reference.sourceLength == 0) {
    return false;
  }
  const auto document = _documentStore->getDocument(reference.sourceDocumentId);
  if (document == nullptr) {
    return false;
  }
  const auto begin = static_cast<std::size_t>(reference.sourceOffset);
  const auto length = static_cast<std::size_t>(reference.sourceLength);
  if (begin >= document->text().size()) {
    return false;
  }
  if (begin > 0 && document->text()[begin - 1] == '.') {
    return true;
  }
  const auto cappedLength = std::min(length, document->text().size() - begin);
  const auto slice = document->textView().substr(begin, cappedLength);
  return slice.find('.') != std::string_view::npos;
}

} // namespace domainmodel::services::lsp
