#include <pegium/core/workspace/DefaultDocuments.hpp>

#include <cassert>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium::workspace {

DocumentId DefaultDocuments::getDocumentId(std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return InvalidDocumentId;
  }
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
  assert(document != nullptr);
  if (document->uri.empty()) {
    throw std::runtime_error("Cannot add a document without URI.");
  }
  if (document->uri != utils::normalize_uri(document->uri)) {
    throw std::runtime_error("Cannot add a document with a non-normalized URI.");
  }

  std::scoped_lock lock(_mutex);
  if (_documents.has(document->uri)) {
    throw std::runtime_error("A document with the same URI is already present.");
  }
  (void)addDocumentLocked(std::move(document));
}

bool DefaultDocuments::hasDocument(std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return false;
  }
  std::scoped_lock lock(_mutex);
  return _documents.has(normalizedUri);
}

std::shared_ptr<Document> DefaultDocuments::getDocument(
    std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return nullptr;
  }
  std::scoped_lock lock(_mutex);
  const auto *document = _documents.find(normalizedUri);
  return document != nullptr ? *document : nullptr;
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
  if (normalizedUri.empty()) {
    return {};
  }
  std::scoped_lock lock(_mutex);
  return _documents.findAll(normalizedUri);
}

std::shared_ptr<Document> DefaultDocuments::getOrCreateDocument(
    std::string_view uri, const utils::CancellationToken &cancelToken) {
  const auto normalizedUri = utils::normalize_uri(uri);
  {
    std::scoped_lock lock(_mutex);
    if (const auto *existing = _documents.find(normalizedUri);
        existing != nullptr) {
      return *existing;
    }
  }

  auto document = shared.workspace.documentFactory->fromUri(
      normalizedUri, cancelToken);
  assert(document != nullptr);
  assert(!document->uri.empty());

  std::scoped_lock lock(_mutex);
  if (const auto *existing = _documents.find(document->uri); existing != nullptr) {
    return *existing;
  }
  return addDocumentLocked(std::move(document));
}

std::shared_ptr<Document> DefaultDocuments::createDocument(
    std::string uri, std::string text,
    const utils::CancellationToken &cancelToken) {
  auto document = shared.workspace.documentFactory->fromString(
      std::move(text), std::move(uri), cancelToken);
  assert(document != nullptr);
  assert(!document->uri.empty());

  std::scoped_lock lock(_mutex);
  if (_documents.has(document->uri)) {
    throw std::runtime_error("A document with the same URI is already present.");
  }
  return addDocumentLocked(std::move(document));
}

std::shared_ptr<Document> DefaultDocuments::deleteDocument(
    std::string_view uri) {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return nullptr;
  }
  std::scoped_lock lock(_mutex);
  return deleteDocumentLocked(normalizedUri);
}

std::shared_ptr<Document> DefaultDocuments::deleteDocument(DocumentId id) {
  if (id == InvalidDocumentId) {
    return nullptr;
  }

  std::scoped_lock lock(_mutex);
  const auto byId = _documentsById.find(id);
  if (byId == _documentsById.end()) {
    return nullptr;
  }
  return deleteDocumentLocked(byId->second->uri);
}

std::vector<std::shared_ptr<Document>> DefaultDocuments::deleteDocuments(
    std::string_view uri) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);
  auto documents = _documents.findAll(normalizedUri);
  for (const auto &document : documents) {
    document->state = DocumentState::Changed;
    _documentsById.erase(document->id);
  }
  _documents.erase(normalizedUri);
  return documents;
}

std::vector<std::shared_ptr<Document>> DefaultDocuments::all() const {
  std::scoped_lock lock(_mutex);
  return _documents.all();
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

std::shared_ptr<Document>
DefaultDocuments::addDocumentLocked(std::shared_ptr<Document> document) {
  assert(document != nullptr);
  document->id = ensureDocumentIdLocked(document->uri);
  _documents.insert(document->uri, document);
  _documentsById.insert_or_assign(document->id, document);
  return document;
}

std::shared_ptr<Document>
DefaultDocuments::deleteDocumentLocked(std::string_view normalizedUri) {
  const auto *existing = _documents.find(normalizedUri);
  if (existing == nullptr) {
    return nullptr;
  }
  auto document = *existing;
  document->state = DocumentState::Changed;
  _documentsById.erase(document->id);
  _documents.erase(normalizedUri);
  return document;
}

} // namespace pegium::workspace
