#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <pegium/core/syntax-tree/CstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/text/Position.hpp>
#include <pegium/core/text/Range.hpp>

namespace pegium::workspace {

class DocumentFactory;

/// One full-text or ranged text document change.
struct TextDocumentContentChangeEvent {
  std::optional<text::Range> range;
  std::optional<TextOffset> rangeLength;
  std::string text;
};

/// Concrete text edit produced by editor-oriented helpers.
struct TextEdit {
  text::Range range;
  std::string newText;
};

/// Immutable-snapshot-backed text document aligned with the LSP text-document
/// contract.
class TextDocument {
  friend class DocumentFactory;

public:
  TextDocument() = delete;
  ~TextDocument();
  TextDocument(const TextDocument &other);
  TextDocument &operator=(const TextDocument &other);
  TextDocument(TextDocument &&other) noexcept;
  TextDocument &operator=(TextDocument &&other) noexcept;

  /// Creates a text document with readonly identity and version metadata.
  [[nodiscard]] static TextDocument create(
      std::string uri, std::string languageId, std::int64_t version,
      std::string content);

  /// Applies content changes in order and returns `document`.
  [[nodiscard]] static TextDocument &update(
      TextDocument &document,
      std::span<const TextDocumentContentChangeEvent> changes,
      std::int64_t version);

  /// Applies `edits` to `document` and returns the resulting text.
  [[nodiscard]] static std::string
  applyEdits(const TextDocument &document, std::span<const TextEdit> edits);

  /// Returns the normalized document URI.
  [[nodiscard]] const std::string &uri() const noexcept { return _uri; }
  /// Returns the document language identifier.
  [[nodiscard]] const std::string &languageId() const noexcept {
    return _languageId;
  }
  /// Returns the current document version.
  [[nodiscard]] std::int64_t version() const noexcept { return _version; }

  /// Returns the full current text as a view into the current snapshot.
  [[nodiscard]] std::string_view getText() const noexcept {
    return _snapshot.view();
  }
  /// Returns the current document text restricted to `range`.
  [[nodiscard]] std::string getText(const text::Range &range) const;

  /// Converts the zero-based position to a zero-based offset.
  [[nodiscard]] TextOffset offsetAt(const text::Position &position) const;
  [[nodiscard]] TextOffset offsetAt(std::uint32_t line,
                                    std::uint32_t character) const;
  /// Converts the zero-based offset to a zero-based position.
  [[nodiscard]] text::Position positionAt(TextOffset offset) const;
  /// Returns the number of lines in the current text.
  [[nodiscard]] std::uint32_t lineCount() const;

private:
  TextDocument(std::string uri, std::string languageId, std::int64_t version,
               text::TextSnapshot snapshot);

  struct Impl;
  [[nodiscard]] Impl &ensureImpl() const;
  void ensureLineIndex() const;
  void invalidateLineIndex() const noexcept;
  void setText(std::string newText);
  [[nodiscard]] text::TextSnapshot snapshot() const noexcept { return _snapshot; }

  std::string _uri;
  std::string _languageId;
  std::int64_t _version = 0;
  text::TextSnapshot _snapshot;
  mutable std::unique_ptr<Impl> _impl;
  mutable std::mutex _lineIndexMutex;
};

} // namespace pegium::workspace
