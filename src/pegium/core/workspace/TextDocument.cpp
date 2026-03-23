#include <pegium/core/workspace/TextDocument.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include <pegium/core/text/Utf8Utf16.hpp>

namespace pegium::workspace {

namespace {

constexpr std::uint32_t kLineIndexChunkSize = 64 * 1024;

TextOffset line_end_offset(std::span<const TextOffset> lineStart, TextOffset size,
                           std::uint32_t line) {
  if (line + 1 < lineStart.size()) {
    return lineStart[line + 1] - 1;
  }
  return size;
}

std::uint32_t utf16_column(std::string_view text, TextOffset lineStart,
                           TextOffset clampedInLine) {
  const auto *bytes =
      reinterpret_cast<const std::byte *>(text.data()) + lineStart;
  const auto *bytesEnd =
      reinterpret_cast<const std::byte *>(text.data()) + clampedInLine;
  std::uint32_t units = 0;

  while (bytes < bytesEnd) {
    while (bytes < bytesEnd &&
           std::to_integer<unsigned char>(*bytes) < 0x80u) {
      ++bytes;
      ++units;
    }
    if (bytes >= bytesEnd) {
      break;
    }

    std::uint32_t advance = 0;
    std::uint32_t utf16Units = 0;
    text::decodeOneUtf8ToUtf16Units(
        bytes, static_cast<std::uint32_t>(bytesEnd - bytes), advance,
        utf16Units);
    bytes += advance;
    units += utf16Units;
  }

  return units;
}

bool edit_starts_after(const TextEdit &left, const TextEdit &right) {
  if (left.range.start.line != right.range.start.line) {
    return left.range.start.line > right.range.start.line;
  }
  if (left.range.start.character != right.range.start.character) {
    return left.range.start.character > right.range.start.character;
  }
  if (left.range.end.line != right.range.end.line) {
    return left.range.end.line > right.range.end.line;
  }
  return left.range.end.character > right.range.end.character;
}

} // namespace

struct TextDocument::Impl {
  const char *lineIndexData = nullptr;
  std::size_t lineIndexSize = 0;
  std::vector<TextOffset> lineStart;
  std::vector<std::uint32_t> chunkLineLowerBound;
  std::uint32_t numChunks = 0;
};

TextDocument::~TextDocument() = default;

TextDocument::TextDocument(const TextDocument &other)
    : _uri(other._uri), _languageId(other._languageId),
      _version(other._version), _snapshot(other._snapshot) {}

TextDocument::TextDocument(TextDocument &&other) noexcept
    : _uri(std::move(other._uri)), _languageId(std::move(other._languageId)),
      _version(other._version), _snapshot(std::move(other._snapshot)),
      _impl(std::move(other._impl)) {}

TextDocument &TextDocument::operator=(const TextDocument &other) {
  if (this == &other) {
    return *this;
  }
  std::scoped_lock lock(_lineIndexMutex);
  _uri = other._uri;
  _languageId = other._languageId;
  _version = other._version;
  _snapshot = other._snapshot;
  _impl.reset();
  return *this;
}

TextDocument &TextDocument::operator=(TextDocument &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  std::scoped_lock lock(_lineIndexMutex);
  _uri = std::move(other._uri);
  _languageId = std::move(other._languageId);
  _version = other._version;
  _snapshot = std::move(other._snapshot);
  _impl = std::move(other._impl);
  return *this;
}

TextDocument::TextDocument(std::string uri, std::string languageId,
                           std::int64_t version, text::TextSnapshot snapshot)
    : _uri(std::move(uri)), _languageId(std::move(languageId)),
      _version(version), _snapshot(std::move(snapshot)) {}

TextDocument TextDocument::create(std::string uri, std::string languageId,
                                  std::int64_t version, std::string content) {
  return TextDocument(std::move(uri), std::move(languageId), version,
                      text::TextSnapshot::own(std::move(content)));
}

TextDocument &TextDocument::update(
    TextDocument &document, std::span<const TextDocumentContentChangeEvent> changes,
    std::int64_t version) {
  if (changes.empty()) {
    document._version = version;
    return document;
  }

  for (const auto &change : changes) {
    if (!change.range.has_value()) {
      document.setText(change.text);
      continue;
    }

    const auto begin = document.offsetAt(change.range->start);
    const auto end = document.offsetAt(change.range->end);
    auto updated = std::string(document.getText());
    updated.replace(begin, end >= begin ? end - begin : 0U,
                    change.text);
    document.setText(std::move(updated));
  }

  document._version = version;
  return document;
}

std::string TextDocument::applyEdits(const TextDocument &document,
                                     std::span<const TextEdit> edits) {
  if (edits.empty()) {
    return std::string(document.getText());
  }

  std::vector<std::reference_wrapper<const TextEdit>> sorted;
  sorted.reserve(edits.size());
  for (const auto &edit : edits) {
    sorted.emplace_back(edit);
  }

  std::ranges::sort(sorted, [](const auto &left, const auto &right) {
    return edit_starts_after(left.get(), right.get());
  });

  auto updated = std::string(document.getText());
  for (const auto &entry : sorted) {
    const auto &edit = entry.get();
    const auto begin = document.offsetAt(edit.range.start);
    const auto end = document.offsetAt(edit.range.end);
    updated.replace(begin, end >= begin ? end - begin : 0U,
                    edit.newText);
  }
  return updated;
}

TextDocument::Impl &TextDocument::ensureImpl() const {
  if (_impl == nullptr) {
    _impl = std::make_unique<Impl>();
  }
  return *_impl;
}

void TextDocument::setText(std::string newText) {
  std::scoped_lock lock(_lineIndexMutex);
  _snapshot = text::TextSnapshot::own(std::move(newText));
  invalidateLineIndex();
}

std::string TextDocument::getText(const text::Range &range) const {
  auto begin = offsetAt(range.start);
  auto end = offsetAt(range.end);
  if (begin > end) {
    std::swap(begin, end);
  }
  return std::string(getText().substr(begin, end - begin));
}

TextOffset TextDocument::offsetAt(const text::Position &position) const {
  return offsetAt(position.line, position.character);
}

TextOffset TextDocument::offsetAt(std::uint32_t line,
                                  std::uint32_t character) const {
  std::scoped_lock lock(_lineIndexMutex);
  ensureLineIndex();
  const auto &impl = ensureImpl();

  const auto size = static_cast<TextOffset>(
      std::min<std::size_t>(getText().size(),
                            std::numeric_limits<TextOffset>::max()));
  if (line >= impl.lineStart.size()) {
    return size;
  }

  const auto start = impl.lineStart[line];
  const auto end = line_end_offset(impl.lineStart, size, line);

  TextOffset index = start;
  std::uint32_t remainingUnits = character;
  while (index < end && remainingUnits > 0) {
    while (index < end && remainingUnits > 0 &&
           static_cast<unsigned char>(getText()[index]) < 0x80u) {
      ++index;
      --remainingUnits;
    }
    if (remainingUnits == 0 || index >= end) {
      break;
    }

    std::uint32_t advance = 0;
    std::uint32_t utf16Units = 0;
    text::decodeOneUtf8ToUtf16Units(
        reinterpret_cast<const std::byte *>(getText().data()) + index,
        end - index, advance, utf16Units);
    index = index + advance;
    remainingUnits =
        remainingUnits > utf16Units ? remainingUnits - utf16Units : 0u;
  }

  return std::min(index, end);
}

text::Position TextDocument::positionAt(TextOffset offset) const {
  std::scoped_lock lock(_lineIndexMutex);
  ensureLineIndex();
  const auto &impl = ensureImpl();

  const auto size = static_cast<TextOffset>(
      std::min<std::size_t>(getText().size(),
                            std::numeric_limits<TextOffset>::max()));
  const auto clamped = std::min(offset, size);

  if (impl.numChunks == 0) {
    return text::Position{0u, 0u};
  }

  const auto chunk =
      std::min<std::uint32_t>(clamped / kLineIndexChunkSize, impl.numChunks - 1);
  const auto lo = impl.chunkLineLowerBound[chunk];
  auto hi = impl.chunkLineLowerBound[chunk + 1] + 1;
  hi = std::min(hi, static_cast<std::uint32_t>(impl.lineStart.size()));

  const auto beginIt = impl.lineStart.begin() + lo;
  const auto endIt = impl.lineStart.begin() + hi;
  const auto it = std::ranges::upper_bound(beginIt, endIt, clamped);
  const auto line = static_cast<std::uint32_t>((it - impl.lineStart.begin()) - 1);
  const auto lineStart = impl.lineStart[line];
  const auto lineEnd = line_end_offset(impl.lineStart, size, line);
  const auto clampedInLine = std::min(std::max(clamped, lineStart), lineEnd);
  return text::Position{
      line, utf16_column(getText(), lineStart, clampedInLine)};
}

std::uint32_t TextDocument::lineCount() const {
  std::scoped_lock lock(_lineIndexMutex);
  ensureLineIndex();
  return static_cast<std::uint32_t>(ensureImpl().lineStart.size());
}

void TextDocument::invalidateLineIndex() const noexcept {
  if (_impl == nullptr) {
    return;
  }
  _impl->lineIndexData = nullptr;
  _impl->lineIndexSize = 0;
  _impl->numChunks = 0;
  _impl->lineStart.clear();
  _impl->chunkLineLowerBound.clear();
}

void TextDocument::ensureLineIndex() const {
  auto &impl = ensureImpl();

  const auto *data = getText().data();
  const auto size = getText().size();
  if (impl.lineIndexData == data && impl.lineIndexSize == size &&
      !impl.lineStart.empty()) {
    return;
  }

  impl.lineStart.clear();
  impl.lineStart.reserve(1024);
  impl.lineStart.push_back(0);

  const auto clampedSize = static_cast<TextOffset>(
      std::min<std::size_t>(size, std::numeric_limits<TextOffset>::max()));
  impl.numChunks =
      (clampedSize + kLineIndexChunkSize - 1) / kLineIndexChunkSize;
  impl.chunkLineLowerBound.assign(static_cast<std::size_t>(impl.numChunks) + 1u,
                                  0u);

  for (std::uint32_t chunk = 0; chunk < impl.numChunks; ++chunk) {
    const auto chunkStart = chunk * kLineIndexChunkSize;
    const auto chunkEnd =
        std::min<TextOffset>(clampedSize, chunkStart + kLineIndexChunkSize);
    impl.chunkLineLowerBound[chunk] =
        static_cast<std::uint32_t>(impl.lineStart.size() - 1);

    for (auto offset = chunkStart; offset < chunkEnd; ++offset) {
      if (getText()[offset] == '\n' && offset + 1 <= clampedSize) {
        impl.lineStart.push_back(offset + 1);
      }
    }
  }
  impl.chunkLineLowerBound[impl.numChunks] =
      impl.lineStart.empty()
          ? 0u
          : static_cast<std::uint32_t>(impl.lineStart.size() - 1);

  impl.lineIndexData = data;
  impl.lineIndexSize = size;
}

} // namespace pegium::workspace
