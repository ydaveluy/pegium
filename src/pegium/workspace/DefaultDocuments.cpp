#include <pegium/workspace/DefaultDocuments.hpp>

#include <ranges>
#include <stdexcept>
#include <utility>

#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/utils/UriUtils.hpp>

namespace pegium::workspace {

DocumentId DefaultDocuments::getDocumentId(std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);
  const auto it = _documentIdsByUri.find(normalizedUri);
  return it == _documentIdsByUri.end() ? InvalidDocumentId : it->second;
}

DocumentId DefaultDocuments::getOrCreateDocumentId(std::string_view uri) {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return InvalidDocumentId;
  }
  std::scoped_lock lock(_mutex);
  return ensureDocumentIdLocked(normalizedUri);
}

void DefaultDocuments::addDocument(std::shared_ptr<Document> document) {
  if (document == nullptr) {
    throw std::runtime_error("Cannot add a document without URI.");
  }
  document->uri = utils::normalize_uri(document->uri);
  if (document->uri.empty()) {
    throw std::runtime_error("Cannot add a document without URI.");
  }

  std::scoped_lock lock(_mutex);
  if (_documents.contains(document->uri)) {
    throw std::runtime_error("A document with the same URI is already present.");
  }
  const auto documentUri = document->uri;
  document->id = ensureDocumentIdLocked(document->uri);
  const auto documentId = document->id;
  _documents.try_emplace(documentUri, document);
  _documentsById.insert_or_assign(documentId, std::move(document));
}

bool DefaultDocuments::hasDocument(std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);
  return _documents.contains(normalizedUri);
}

std::shared_ptr<Document> DefaultDocuments::getDocument(
    std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);
  const auto it = _documents.find(normalizedUri);
  return it == _documents.end() ? nullptr : it->second;
}

std::shared_ptr<Document> DefaultDocuments::getDocument(DocumentId id) const {
  if (id == InvalidDocumentId) {
    return nullptr;
  }
  std::scoped_lock lock(_mutex);
  const auto it = _documentsById.find(id);
  return it == _documentsById.end() ? nullptr : it->second;
}

std::string DefaultDocuments::getDocumentUri(DocumentId id) const {
  if (id == InvalidDocumentId) {
    return {};
  }
  std::scoped_lock lock(_mutex);
  const auto it = _documentUrisById.find(id);
  return it == _documentUrisById.end() ? std::string{} : it->second;
}

std::vector<std::shared_ptr<Document>> DefaultDocuments::getDocuments(
    std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);

  std::vector<std::shared_ptr<Document>> documents;
  for (const auto &[documentUri, document] : _documents) {
    if (matches_uri_or_folder(documentUri, normalizedUri)) {
      documents.push_back(document);
    }
  }
  return documents;
}

std::shared_ptr<Document> DefaultDocuments::getOrCreateDocument(
    std::string_view uri, const utils::CancellationToken &cancelToken) {
  const auto normalizedUri = utils::normalize_uri(uri);
  {
    std::scoped_lock lock(_mutex);
    const auto it = _documents.find(normalizedUri);
    if (it != _documents.end()) {
      return it->second;
    }
  }

  if (sharedCoreServices.workspace.documentFactory == nullptr) {
    return nullptr;
  }

  auto document = sharedCoreServices.workspace.documentFactory->fromUri(
      normalizedUri, cancelToken);
  if (document == nullptr || document->uri.empty()) {
    return nullptr;
  }

  std::scoped_lock lock(_mutex);
  document->uri = utils::normalize_uri(document->uri);
  document->id = ensureDocumentIdLocked(document->uri);
  const auto insertedDocument = _documents.try_emplace(document->uri, document);
  _documentsById.insert_or_assign(insertedDocument.first->second->id,
                                  insertedDocument.first->second);
  return insertedDocument.first->second;
}

std::shared_ptr<Document> DefaultDocuments::createDocument(
    std::string uri, std::string text, std::string languageId,
    const utils::CancellationToken &cancelToken) {
  if (sharedCoreServices.workspace.documentFactory == nullptr) {
    return nullptr;
  }

  auto document = sharedCoreServices.workspace.documentFactory->fromString(
      std::move(text), std::move(uri), std::move(languageId), std::nullopt,
      cancelToken);
  if (document == nullptr || document->uri.empty()) {
    return nullptr;
  }

  addDocument(document);
  return document;
}

std::shared_ptr<Document> DefaultDocuments::invalidateDocument(
    std::string_view uri) {
  auto document = getDocument(uri);
  if (document == nullptr) {
    return nullptr;
  }
  if (sharedCoreServices.workspace.documentBuilder != nullptr) {
    sharedCoreServices.workspace.documentBuilder->resetToState(
        *document, DocumentState::Changed);
  } else {
    document->state = DocumentState::Changed;
  }
  return document;
}

std::shared_ptr<Document> DefaultDocuments::deleteDocument(
    std::string_view uri) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);
  const auto it = _documents.find(normalizedUri);
  if (it == _documents.end()) {
    return nullptr;
  }

  auto document = std::move(it->second);
  document->state = DocumentState::Changed;
  _documentsById.erase(document->id);
  _documents.erase(it);
  return document;
}

std::shared_ptr<Document> DefaultDocuments::deleteDocument(DocumentId id) {
  if (id == InvalidDocumentId) {
    return nullptr;
  }

  std::scoped_lock lock(_mutex);
  const auto byId = _documentsById.find(id);
  if (byId == _documentsById.end() || byId->second == nullptr) {
    return nullptr;
  }

  auto document = std::move(byId->second);
  document->state = DocumentState::Changed;
  _documentsById.erase(byId);
  if (!document->uri.empty()) {
    _documents.erase(document->uri);
  }
  return document;
}

std::vector<std::shared_ptr<Document>> DefaultDocuments::deleteDocuments(
    std::string_view uri) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);

  std::vector<std::string> keys;
  for (const auto &entry : _documents) {
    if (matches_uri_or_folder(entry.first, normalizedUri)) {
      keys.push_back(entry.first);
    }
  }

  std::vector<std::shared_ptr<Document>> documents;
  documents.reserve(keys.size());
  for (const auto &key : keys) {
    auto it = _documents.find(key);
    if (it == _documents.end()) {
      continue;
    }
    it->second->state = DocumentState::Changed;
    _documentsById.erase(it->second->id);
    documents.push_back(std::move(it->second));
    _documents.erase(it);
  }
  return documents;
}

utils::stream<std::shared_ptr<Document>> DefaultDocuments::all() const {
  std::scoped_lock lock(_mutex);

  std::vector<std::shared_ptr<Document>> documents;
  documents.reserve(_documents.size());
  for (const auto &document : std::views::values(_documents)) {
    documents.push_back(document);
  }
  return utils::make_stream<std::shared_ptr<Document>>(std::move(documents));
}

void DefaultDocuments::clear() {
  std::scoped_lock lock(_mutex);
  _documents.clear();
  _documentsById.clear();
  _documentIdsByUri.clear();
  _documentUrisById.clear();
  _nextDocumentId = 1;
}

DocumentId DefaultDocuments::ensureDocumentIdLocked(std::string_view uri) {
  const auto key = std::string(uri);
  if (const auto it = _documentIdsByUri.find(key); it != _documentIdsByUri.end()) {
    return it->second;
  }
  const auto id = _nextDocumentId++;
  const auto insertedId = _documentIdsByUri.try_emplace(key, id);
  _documentUrisById.try_emplace(id, insertedId.first->first);
  return id;
}

bool DefaultDocuments::matches_uri_or_folder(std::string_view candidate,
                                             std::string_view uri) {
  if (candidate == uri) {
    return true;
  }
  if (uri.empty() || candidate.size() <= uri.size()) {
    return false;
  }
  if (!candidate.starts_with(uri)) {
    return false;
  }
  const auto separator = candidate[uri.size()];
  return separator == '/' || separator == '\\';
}

} // namespace pegium::workspace
