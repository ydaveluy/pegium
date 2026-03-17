#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <pegium/syntax-tree/CstNode.hpp>
#include <pegium/text/Position.hpp>
#include <pegium/text/Range.hpp>

namespace pegium::workspace {

using TextDocumentContentChangeRange = text::Range;

struct TextDocumentContentChange {
  std::optional<TextDocumentContentChangeRange> range;
  std::string text;
};

class TextDocument {
public:
  std::string uri;
  std::string languageId;

  TextDocument();
  ~TextDocument();
  TextDocument(const TextDocument &other);
  TextDocument &operator=(const TextDocument &other);
  TextDocument(TextDocument &&other) = delete;
  TextDocument &operator=(TextDocument &&other) = delete;

  [[nodiscard]] const std::string &text() const noexcept { return _text; }
  [[nodiscard]] std::string_view textView() const noexcept { return _text; }

  [[nodiscard]] std::uint64_t revision() const noexcept { return _revision; }
  [[nodiscard]] std::optional<std::int64_t> clientVersion() const noexcept {
    return _clientVersion;
  }
  void setClientVersion(std::optional<std::int64_t> version) noexcept {
    _clientVersion = version;
  }

  void replaceText(std::string newText);
  void applyContentChanges(std::span<const TextDocumentContentChange> changes);

  [[nodiscard]] TextOffset positionToOffset(const text::Position &position) const;
  [[nodiscard]] TextOffset positionToOffset(std::uint32_t line,
                                            std::uint32_t character) const;
  [[nodiscard]] text::Position offsetToPosition(TextOffset offset) const;

private:
  struct Impl;
  [[nodiscard]] Impl &ensureImpl() const;
  void ensureLineIndex() const;
  void invalidateLineIndex() const noexcept;

  std::string _text;
  std::uint64_t _revision = 0;
  std::optional<std::int64_t> _clientVersion;
  mutable std::unique_ptr<Impl> _impl;
  mutable std::mutex _lineIndexMutex;
};

} // namespace pegium::workspace
