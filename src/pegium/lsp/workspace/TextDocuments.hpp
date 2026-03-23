#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/core/utils/Disposable.hpp>
#include <pegium/core/workspace/TextDocumentEvents.hpp>
#include <pegium/core/workspace/TextDocumentProvider.hpp>

namespace lsp {
class MessageHandler;
}

namespace pegium {

/// Tracks opened text documents and exposes their lifecycle events.
class TextDocuments : public workspace::TextDocumentProvider {
public:
  virtual ~TextDocuments() noexcept = default;

  /// Stores an opened text document. `document` must be non-null and must
  /// carry a normalized non-empty URI.
  [[nodiscard]] virtual bool
  set(std::shared_ptr<workspace::TextDocument> document) = 0;
  /// Removes the tracked document identified by `uri`.
  virtual void remove(std::string_view uri) = 0;
  /// Returns all currently tracked document snapshots.
  [[nodiscard]] virtual std::vector<std::shared_ptr<workspace::TextDocument>>
  all() const = 0;
  /// Returns the URIs of all currently tracked documents.
  [[nodiscard]] virtual std::vector<std::string> keys() const = 0;
  /// Hooks the store to LSP text-document notifications.
  virtual utils::ScopedDisposable
  listen(::lsp::MessageHandler &messageHandler,
         std::function<void()> ensureInitialized = {}) = 0;

  /// Subscribes to open-document events.
  virtual utils::ScopedDisposable onDidOpen(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener) = 0;
  /// Subscribes to content-change events.
  virtual utils::ScopedDisposable onDidChangeContent(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener) = 0;
  /// Subscribes to save events.
  virtual utils::ScopedDisposable onDidSave(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener) = 0;
  /// Subscribes to close-document events.
  virtual utils::ScopedDisposable onDidClose(
      std::function<void(const workspace::TextDocumentChangeEvent &)> listener) = 0;
  /// Subscribes to will-save events.
  ///
  /// Only tracked documents produce this event; `event.document` is never
  /// null.
  virtual utils::ScopedDisposable onWillSave(
      std::function<void(const workspace::TextDocumentWillSaveEvent &)> listener) = 0;
  /// Subscribes to synchronous will-save edit requests.
  ///
  /// Only tracked documents produce this callback; `event.document` is never
  /// null.
  virtual utils::ScopedDisposable onWillSaveWaitUntil(
      std::function<std::vector<workspace::TextEdit>(
          const workspace::TextDocumentWillSaveEvent &)>
          listener) = 0;
};

} // namespace pegium
