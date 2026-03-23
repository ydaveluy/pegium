#pragma once

#include <memory>
#include <string>

#include <pegium/core/workspace/TextDocument.hpp>

namespace pegium::workspace {

/// Base text-document event carrying the affected document.
struct TextDocumentChangeEvent {
  /// `document` is non-null and has a normalized non-empty URI for events
  /// emitted by `TextDocuments`.
  std::shared_ptr<TextDocument> document;
};

/// Reason reported for a text-document save request.
enum class TextDocumentSaveReason {
  Manual,
  AfterDelay,
  FocusOut,
};

/// Event emitted before a text document is saved.
struct TextDocumentWillSaveEvent {
  /// `document` is non-null and has a normalized non-empty URI for events
  /// emitted by `TextDocuments`.
  std::shared_ptr<TextDocument> document;
  TextDocumentSaveReason reason = TextDocumentSaveReason::Manual;
};

} // namespace pegium::workspace
