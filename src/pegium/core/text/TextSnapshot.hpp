#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace pegium::text {

/// Shared immutable text buffer used by parser and CST snapshots.
///
/// `TextSnapshot` gives Pegium one stable owner for source text while keeping
/// copies cheap. Workspace documents can share their current snapshot with the
/// parser, and standalone parses can materialize one on demand.
class TextSnapshot {
public:
  TextSnapshot() noexcept : _text(empty_text()) {}

  explicit TextSnapshot(std::shared_ptr<const std::string> text) noexcept
      : _text(text == nullptr ? empty_text() : std::move(text)) {}

  [[nodiscard]] static TextSnapshot copy(std::string_view text) {
    return TextSnapshot(std::make_shared<const std::string>(text));
  }

  [[nodiscard]] static TextSnapshot own(std::string text) {
    return TextSnapshot(
        std::make_shared<const std::string>(std::move(text)));
  }

  [[nodiscard]] std::string_view view() const noexcept { return *_text; }
  [[nodiscard]] const std::string &str() const noexcept { return *_text; }
  [[nodiscard]] bool empty() const noexcept { return _text->empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return _text->size(); }

private:
  [[nodiscard]] static std::shared_ptr<const std::string> empty_text() noexcept {
    static const auto empty = std::make_shared<const std::string>();
    return empty;
  }

  std::shared_ptr<const std::string> _text;
};

} // namespace pegium::text
