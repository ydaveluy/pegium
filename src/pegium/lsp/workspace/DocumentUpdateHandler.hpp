#pragma once

#include <lsp/types.h>

#include <pegium/lsp/workspace/TextDocuments.hpp>
#include <pegium/core/utils/Event.hpp>

namespace pegium {

using TextDocumentChangeEvent = workspace::TextDocumentChangeEvent;

/// Save-event payload forwarded to document update handlers.
struct TextDocumentWillSaveEvent {
  /// `document` is non-null and has a normalized non-empty URI for events
  /// forwarded from `TextDocuments`.
  std::shared_ptr<workspace::TextDocument> document;
  ::lsp::TextDocumentSaveReason reason = ::lsp::TextDocumentSaveReason::Manual;
};

/// Handles document and file-system events that impact workspace state.
class DocumentUpdateHandler {
public:
  virtual ~DocumentUpdateHandler() noexcept = default;

  /// Returns whether the handler expects `didSave` notifications.
  [[nodiscard]] virtual bool supportsDidSaveDocument() const noexcept {
    return false;
  }

  /// Returns whether the handler expects `willSave` notifications.
  [[nodiscard]] virtual bool supportsWillSaveDocument() const noexcept {
    return false;
  }

  /// Returns whether the handler expects `willSaveWaitUntil` requests.
  [[nodiscard]] virtual bool
  supportsWillSaveDocumentWaitUntil() const noexcept {
    return false;
  }

  /// Handles document-open events.
  virtual void didOpenDocument(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  /// Handles content-change events.
  virtual void didChangeContent(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  /// Handles pre-save notifications.
  virtual void willSaveDocument(const TextDocumentWillSaveEvent &event) {
    (void)event;
  }
  /// Produces edits for `willSaveWaitUntil` when supported.
  [[nodiscard]] virtual ::lsp::Array<::lsp::TextEdit>
  willSaveDocumentWaitUntil(const TextDocumentWillSaveEvent &event) {
    (void)event;
    return {};
  }
  /// Handles post-save notifications.
  virtual void didSaveDocument(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  /// Handles document-close events.
  virtual void didCloseDocument(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  /// Handles external watched-file changes.
  virtual void
  didChangeWatchedFiles(const ::lsp::DidChangeWatchedFilesParams &params) {
    (void)params;
  }
  /// Subscribes to watched-file changes observed by the handler.
  virtual utils::ScopedDisposable onWatchedFilesChange(
      const std::function<void(const ::lsp::DidChangeWatchedFilesParams &)>
          &listener) {
    (void)listener;
    return {};
  }
};

} // namespace pegium
