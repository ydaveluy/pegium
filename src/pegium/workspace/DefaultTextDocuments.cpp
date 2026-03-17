#include <pegium/workspace/DefaultTextDocuments.hpp>

#include <iterator>
#include <unordered_map>
#include <utility>

#include <pegium/utils/UriUtils.hpp>

namespace pegium::workspace {

std::shared_ptr<const TextDocument> DefaultTextDocuments::open(
    std::string uri, std::string languageId, std::string text,
    std::optional<std::int64_t> clientVersion) {
  uri = utils::normalize_uri(uri);
  if (uri.empty()) {
    return nullptr;
  }

  auto document = std::make_shared<TextDocument>();
  document->uri = std::move(uri);
  document->languageId = std::move(languageId);
  document->replaceText(std::move(text));
  document->setClientVersion(clientVersion);

  {
    std::scoped_lock lock(_mutex);
    _documents.insert_or_assign(document->uri, document);
  }
  emitDidOpen(document);
  emitDidChangeContent(document);
  return document;
}

std::shared_ptr<const TextDocument> DefaultTextDocuments::replaceText(
    std::string_view uri, std::string text, std::string languageId,
    std::optional<std::int64_t> clientVersion) {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return nullptr;
  }

  std::scoped_lock lock(_mutex);
  const auto it = _documents.find(normalizedUri);
  auto document =
      it == _documents.end() ? std::make_shared<TextDocument>()
                             : std::make_shared<TextDocument>(*it->second);
  document->uri = normalizedUri;
  if (!languageId.empty()) {
    document->languageId = std::move(languageId);
  }
  document->replaceText(std::move(text));
  document->setClientVersion(clientVersion);
  _documents.insert_or_assign(document->uri, document);
  return document;
}

std::shared_ptr<const TextDocument> DefaultTextDocuments::applyContentChanges(
    std::string_view uri, std::span<const TextDocumentContentChange> changes,
    std::optional<std::int64_t> clientVersion) {
  const auto normalizedUri = utils::normalize_uri(uri);
  if (normalizedUri.empty()) {
    return nullptr;
  }

  std::shared_ptr<const TextDocument> document;
  {
    std::scoped_lock lock(_mutex);
    const auto it = _documents.find(normalizedUri);
    if (it == _documents.end()) {
      return nullptr;
    }

    auto updated = std::make_shared<TextDocument>(*it->second);
    updated->applyContentChanges(changes);
    updated->setClientVersion(clientVersion);
    _documents.insert_or_assign(updated->uri, updated);
    document = std::move(updated);
  }
  emitDidChangeContent(document);
  return document;
}

std::shared_ptr<const TextDocument>
DefaultTextDocuments::save(std::string_view uri, std::optional<std::string> text) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::shared_ptr<const TextDocument> document;
  {
    std::scoped_lock lock(_mutex);
    const auto it = _documents.find(normalizedUri);
    if (it == _documents.end()) {
      return nullptr;
    }

    if (text.has_value()) {
      auto updated = std::make_shared<TextDocument>(*it->second);
      updated->replaceText(std::move(*text));
      _documents.insert_or_assign(updated->uri, updated);
      document = std::move(updated);
    } else {
      document = it->second;
    }
  }

  emitDidSave(document);
  return document;
}

bool DefaultTextDocuments::close(std::string_view uri) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::shared_ptr<const TextDocument> document;
  {
    std::scoped_lock lock(_mutex);
    const auto it = _documents.find(normalizedUri);
    if (it == _documents.end()) {
      return false;
    }
    document = it->second;
    _documents.erase(it);
  }
  emitDidClose(document);
  return true;
}

bool DefaultTextDocuments::willSave(std::string_view uri,
                                    TextDocumentSaveReason reason) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::shared_ptr<const TextDocument> document;
  {
    std::scoped_lock lock(_mutex);
    const auto it = _documents.find(normalizedUri);
    if (it == _documents.end()) {
      return false;
    }
    document = it->second;
  }

  emitWillSave({.document = document, .reason = reason});
  return true;
}

std::vector<TextDocumentEdit>
DefaultTextDocuments::willSaveWaitUntil(std::string_view uri,
                                        TextDocumentSaveReason reason) {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::shared_ptr<const TextDocument> document;
  {
    std::scoped_lock lock(_mutex);
    const auto it = _documents.find(normalizedUri);
    if (it == _documents.end()) {
      return {};
    }
    document = it->second;
  }

  return emitWillSaveWaitUntil({.document = document, .reason = reason});
}

std::shared_ptr<const TextDocument> DefaultTextDocuments::get(
    std::string_view uri) const {
  const auto normalizedUri = utils::normalize_uri(uri);
  std::scoped_lock lock(_mutex);
  const auto it = _documents.find(normalizedUri);
  if (it == _documents.end()) {
    return nullptr;
  }
  return it->second;
}

utils::ScopedDisposable DefaultTextDocuments::onDidOpen(
    std::function<void(const TextDocumentEvent &)> listener) {
  return _onDidOpen.on(std::move(listener));
}

utils::ScopedDisposable DefaultTextDocuments::onDidChangeContent(
    std::function<void(const TextDocumentEvent &)> listener) {
  return _onDidChangeContent.on(std::move(listener));
}

utils::ScopedDisposable DefaultTextDocuments::onDidSave(
    std::function<void(const TextDocumentEvent &)> listener) {
  return _onDidSave.on(std::move(listener));
}

utils::ScopedDisposable DefaultTextDocuments::onDidClose(
    std::function<void(const TextDocumentEvent &)> listener) {
  return _onDidClose.on(std::move(listener));
}

utils::ScopedDisposable DefaultTextDocuments::onWillSave(
    std::function<void(const TextDocumentWillSaveEvent &)> listener) {
  return _onWillSave.on(std::move(listener));
}

utils::ScopedDisposable DefaultTextDocuments::onWillSaveWaitUntil(
    std::function<std::vector<TextDocumentEdit>(const TextDocumentWillSaveEvent &)>
        listener) {
  std::size_t id = 0;
  {
    std::scoped_lock lock(_mutex);
    id = _nextWillSaveWaitUntilId++;
    _onWillSaveWaitUntil.emplace(id, std::move(listener));
  }

  return utils::ScopedDisposable([this, id]() {
    std::scoped_lock lock(_mutex);
    _onWillSaveWaitUntil.erase(id);
  });
}

void DefaultTextDocuments::clear() {
  std::scoped_lock lock(_mutex);
  _documents.clear();
}

void DefaultTextDocuments::emitDidOpen(
    const std::shared_ptr<const TextDocument> &document) {
  if (document != nullptr) {
    _onDidOpen.emit({.document = document});
  }
}

void DefaultTextDocuments::emitDidChangeContent(
    const std::shared_ptr<const TextDocument> &document) {
  if (document != nullptr) {
    _onDidChangeContent.emit({.document = document});
  }
}

void DefaultTextDocuments::emitDidSave(
    const std::shared_ptr<const TextDocument> &document) {
  if (document != nullptr) {
    _onDidSave.emit({.document = document});
  }
}

void DefaultTextDocuments::emitDidClose(
    const std::shared_ptr<const TextDocument> &document) {
  if (document != nullptr) {
    _onDidClose.emit({.document = document});
  }
}

void DefaultTextDocuments::emitWillSave(const TextDocumentWillSaveEvent &event) {
  if (event.document != nullptr) {
    _onWillSave.emit(event);
  }
}

std::vector<TextDocumentEdit>
DefaultTextDocuments::emitWillSaveWaitUntil(
    const TextDocumentWillSaveEvent &event) const {
  if (event.document == nullptr) {
    return {};
  }

  std::unordered_map<
      std::size_t,
      std::function<std::vector<TextDocumentEdit>(const TextDocumentWillSaveEvent &)>>
      listenersCopy;
  {
    std::scoped_lock lock(_mutex);
    listenersCopy = _onWillSaveWaitUntil;
  }

  std::vector<TextDocumentEdit> edits;
  for (const auto &[id, listener] : listenersCopy) {
    (void)id;
    if (!listener) {
      continue;
    }
    auto listenerEdits = listener(event);
    if (!listenerEdits.empty()) {
      edits.insert(edits.end(),
                   std::make_move_iterator(listenerEdits.begin()),
                   std::make_move_iterator(listenerEdits.end()));
    }
  }
  return edits;
}

} // namespace pegium::workspace
