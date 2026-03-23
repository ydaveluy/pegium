#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/lsp/workspace/TextDocuments.hpp>
#include <pegium/core/utils/Event.hpp>

namespace pegium {

/// Default in-memory store for opened LSP text documents.
class DefaultTextDocuments : public TextDocuments {
public:
  /// Returns the current snapshot for `uri`, or `nullptr` when absent.
  [[nodiscard]] std::shared_ptr<workspace::TextDocument>
  get(std::string_view uri) const override;
  /// Stores or replaces one opened document snapshot.
  [[nodiscard]] bool
  set(std::shared_ptr<workspace::TextDocument> document) override;
  /// Removes the snapshot tracked for `uri`.
  void remove(std::string_view uri) override;
  /// Returns all tracked document snapshots.
  [[nodiscard]] std::vector<std::shared_ptr<workspace::TextDocument>>
  all() const override;
  /// Returns the URIs of all tracked documents.
  [[nodiscard]] std::vector<std::string> keys() const override;
  /// Registers LSP text-document notifications on `messageHandler`.
  utils::ScopedDisposable
  listen(::lsp::MessageHandler &messageHandler,
         std::function<void()> ensureInitialized = {}) override;

  /// Subscribes to open-document events.
  utils::ScopedDisposable
  onDidOpen(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener)
      override;
  /// Subscribes to content-change events.
  utils::ScopedDisposable onDidChangeContent(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener)
      override;
  /// Subscribes to save events.
  utils::ScopedDisposable
  onDidSave(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener)
      override;
  /// Subscribes to close-document events.
  utils::ScopedDisposable
  onDidClose(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener)
      override;
  /// Subscribes to will-save events.
  utils::ScopedDisposable onWillSave(
      std::function<void(const workspace::TextDocumentWillSaveEvent &)> listener)
      override;
  /// Subscribes to synchronous will-save edit requests.
  utils::ScopedDisposable onWillSaveWaitUntil(
      std::function<std::vector<workspace::TextEdit>(
          const workspace::TextDocumentWillSaveEvent &)>
          listener) override;

private:
  void emitWillSave(
      const std::shared_ptr<workspace::TextDocument> &document,
      workspace::TextDocumentSaveReason reason);
  void emitDidOpen(const std::shared_ptr<workspace::TextDocument> &document);
  void emitDidChangeContent(
      const std::shared_ptr<workspace::TextDocument> &document);
  void emitDidSave(const std::shared_ptr<workspace::TextDocument> &document);
  void emitDidClose(const std::shared_ptr<workspace::TextDocument> &document);
  [[nodiscard]] std::vector<workspace::TextEdit>
  emitWillSaveWaitUntil(
      const std::shared_ptr<workspace::TextDocument> &document,
      workspace::TextDocumentSaveReason reason) const;

  mutable std::mutex _mutex;
  std::unordered_map<std::string, std::shared_ptr<workspace::TextDocument>>
      _documents;
  utils::EventEmitter<workspace::TextDocumentChangeEvent> _onDidOpen;
  utils::EventEmitter<workspace::TextDocumentChangeEvent> _onDidChangeContent;
  utils::EventEmitter<workspace::TextDocumentChangeEvent> _onDidSave;
  utils::EventEmitter<workspace::TextDocumentChangeEvent> _onDidClose;
  utils::EventEmitter<workspace::TextDocumentWillSaveEvent> _onWillSave;
  std::function<std::vector<workspace::TextEdit>(
      const workspace::TextDocumentWillSaveEvent &)>
      _onWillSaveWaitUntil;
  std::size_t _nextWillSaveWaitUntilId = 0;
};

} // namespace pegium
