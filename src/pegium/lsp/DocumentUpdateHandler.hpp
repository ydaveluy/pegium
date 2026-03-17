#pragma once

#include <lsp/types.h>

#include <pegium/utils/Event.hpp>
#include <pegium/workspace/TextDocuments.hpp>

namespace pegium::lsp {

using TextDocumentChangeEvent = workspace::TextDocumentEvent;

struct TextDocumentWillSaveEvent {
  std::shared_ptr<const workspace::TextDocument> document;
  ::lsp::TextDocumentSaveReason reason = ::lsp::TextDocumentSaveReason::Manual;
};

class DocumentUpdateHandler {
public:
  virtual ~DocumentUpdateHandler() noexcept = default;

  [[nodiscard]] virtual bool supportsDidOpenDocument() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsDidChangeContent() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsDidSaveDocument() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsDidCloseDocument() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsWillSaveDocument() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool
  supportsWillSaveDocumentWaitUntil() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsDidChangeWatchedFiles() const noexcept {
    return false;
  }

  virtual void didOpenDocument(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  virtual void didChangeContent(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  virtual void willSaveDocument(const TextDocumentWillSaveEvent &event) {
    (void)event;
  }
  [[nodiscard]] virtual ::lsp::Array<::lsp::TextEdit>
  willSaveDocumentWaitUntil(const TextDocumentWillSaveEvent &event) {
    (void)event;
    return {};
  }
  virtual void didSaveDocument(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  virtual void didCloseDocument(const TextDocumentChangeEvent &event) {
    (void)event;
  }
  virtual void
  didChangeWatchedFiles(const ::lsp::DidChangeWatchedFilesParams &params) {
    (void)params;
  }
  virtual utils::ScopedDisposable onWatchedFilesChange(
      std::function<void(const ::lsp::DidChangeWatchedFilesParams &)>
          listener) {
    (void)listener;
    return {};
  }
};

} // namespace pegium::lsp
