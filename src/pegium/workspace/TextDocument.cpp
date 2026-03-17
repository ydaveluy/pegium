#include <pegium/workspace/TextDocument.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <pegium/text/Utf8Utf16.hpp>

namespace pegium::workspace {

namespace {

constexpr std::uint32_t kLineIndexChunkSize = 64 * 1024;

} // namespace

struct TextDocument::Impl {
  const char *lineIndexData = nullptr;
  std::size_t lineIndexSize = 0;
  std::vector<TextOffset> lineStart;
  std::vector<std::uint32_t> chunkLineLowerBound;
  std::uint32_t numChunks = 0;
};

TextDocument::TextDocument() = default;
TextDocument::~TextDocument() = default;

TextDocument::TextDocument(const TextDocument &other)
    : uri(other.uri), languageId(other.languageId), _text(other._text),
      _revision(other._revision), _clientVersion(other._clientVersion) {}

TextDocument &TextDocument::operator=(const TextDocument &other) {
  if (this == &other) {
    return *this;
  }
  std::scoped_lock lock(_lineIndexMutex);
  uri = other.uri;
  languageId = other.languageId;
  _text = other._text;
  _revision = other._revision;
  _clientVersion = other._clientVersion;
  _impl.reset();
  return *this;
}

TextDocument::Impl &TextDocument::ensureImpl() const {
  if (_impl == nullptr) {
    _impl = std::make_unique<Impl>();
  }
  return *_impl;
}

void TextDocument::replaceText(std::string newText) {
  std::scoped_lock lock(_lineIndexMutex);
  _text = std::move(newText);
  ++_revision;
  invalidateLineIndex();
}

void TextDocument::applyContentChanges(
    std::span<const TextDocumentContentChange> changes) {
  if (changes.empty()) {
    return;
  }

  for (const auto &change : changes) {
    if (!change.range.has_value()) {
      replaceText(change.text);
      continue;
    }

    const auto begin = positionToOffset(change.range->start);
    const auto end = positionToOffset(change.range->end);

    std::scoped_lock lock(_lineIndexMutex);
    _text.replace(static_cast<std::size_t>(begin),
                  static_cast<std::size_t>(end >= begin ? end - begin : 0),
                  change.text);
    ++_revision;
    invalidateLineIndex();
  }
}

TextOffset TextDocument::positionToOffset(const text::Position &position) const {
  return positionToOffset(position.line, position.character);
}

TextOffset TextDocument::positionToOffset(std::uint32_t line,
                                          std::uint32_t character) const {
  std::scoped_lock lock(_lineIndexMutex);
  ensureLineIndex();
  const auto &impl = ensureImpl();

  const auto size = static_cast<TextOffset>(
      std::min<std::size_t>(_text.size(), std::numeric_limits<TextOffset>::max()));
  if (line >= impl.lineStart.size()) {
    return size;
  }

  const auto start = impl.lineStart[line];
  const auto end = [&impl, size, line]() -> TextOffset {
    if (line + 1 < impl.lineStart.size()) {
      return static_cast<TextOffset>(impl.lineStart[line + 1] - 1);
    }
    return size;
  }();

  TextOffset index = start;
  std::uint32_t remainingUnits = character;
  while (index < end && remainingUnits > 0) {
    while (index < end && remainingUnits > 0 &&
           static_cast<unsigned char>(_text[static_cast<std::size_t>(index)]) <
               0x80u) {
      ++index;
      --remainingUnits;
    }
    if (remainingUnits == 0 || index >= end) {
      break;
    }

    std::uint32_t advance = 0;
    std::uint32_t utf16Units = 0;
    text::decodeOneUtf8ToUtf16Units(
        reinterpret_cast<const std::uint8_t *>(_text.data()) + index,
        static_cast<std::uint32_t>(end - index), advance, utf16Units);
    index = static_cast<TextOffset>(index + advance);
    remainingUnits =
        remainingUnits > utf16Units ? remainingUnits - utf16Units : 0u;
  }

  return std::min(index, end);
}

text::Position TextDocument::offsetToPosition(TextOffset offset) const {
  std::scoped_lock lock(_lineIndexMutex);
  ensureLineIndex();
  const auto &impl = ensureImpl();

  const auto size = static_cast<TextOffset>(
      std::min<std::size_t>(_text.size(), std::numeric_limits<TextOffset>::max()));
  const auto clamped = std::min(offset, size);
  if (impl.lineStart.size() <= 1) {
    return text::Position{0u, clamped};
  }

  const auto chunk =
      impl.numChunks == 0
          ? 0u
          : std::min<std::uint32_t>(clamped / kLineIndexChunkSize,
                                    impl.numChunks - 1);
  const auto lo = impl.chunkLineLowerBound[chunk];
  auto hi = static_cast<std::uint32_t>(impl.chunkLineLowerBound[chunk + 1] + 1);
  hi = std::min(hi, static_cast<std::uint32_t>(impl.lineStart.size()));

  const auto beginIt = impl.lineStart.begin() + static_cast<std::ptrdiff_t>(lo);
  const auto endIt = impl.lineStart.begin() + static_cast<std::ptrdiff_t>(hi);
  const auto it = std::upper_bound(beginIt, endIt, clamped);
  const auto line = static_cast<std::uint32_t>((it - impl.lineStart.begin()) - 1);
  const auto lineStart = impl.lineStart[line];
  const auto lineEnd = [&impl, size, line]() -> TextOffset {
    if (line + 1 < impl.lineStart.size()) {
      return static_cast<TextOffset>(impl.lineStart[line + 1] - 1);
    }
    return size;
  }();
  const auto clampedInLine = std::min(std::max(clamped, lineStart), lineEnd);

  const auto column = [&]() -> std::uint32_t {
    const auto *bytes =
        reinterpret_cast<const std::uint8_t *>(_text.data()) + lineStart;
    const auto *bytesEnd =
        reinterpret_cast<const std::uint8_t *>(_text.data()) + clampedInLine;
    std::uint32_t units = 0;

    while (bytes < bytesEnd) {
      while (bytes < bytesEnd && *bytes < 0x80u) {
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
  }();

  return text::Position{line, column};
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

  const auto *data = _text.data();
  const auto size = _text.size();
  if (impl.lineIndexData == data && impl.lineIndexSize == size &&
      !impl.lineStart.empty()) {
    return;
  }

  impl.lineStart.clear();
  impl.lineStart.reserve(1024);
  impl.lineStart.push_back(0);

  const auto clampedSize = static_cast<TextOffset>(
      std::min<std::size_t>(size, std::numeric_limits<TextOffset>::max()));
  impl.numChunks = static_cast<std::uint32_t>(
      (clampedSize + kLineIndexChunkSize - 1) / kLineIndexChunkSize);
  impl.chunkLineLowerBound.assign(static_cast<std::size_t>(impl.numChunks) + 1u,
                                  0u);

  for (std::uint32_t chunk = 0; chunk < impl.numChunks; ++chunk) {
    const auto chunkStart = static_cast<TextOffset>(chunk * kLineIndexChunkSize);
    const auto chunkEnd =
        std::min<TextOffset>(clampedSize, chunkStart + kLineIndexChunkSize);
    impl.chunkLineLowerBound[chunk] =
        static_cast<std::uint32_t>(impl.lineStart.size() - 1);

    for (auto offset = chunkStart; offset < chunkEnd; ++offset) {
      if (_text[static_cast<std::size_t>(offset)] == '\n' &&
          offset + 1 <= clampedSize) {
        impl.lineStart.push_back(offset + 1);
      }
    }
  }
  impl.chunkLineLowerBound[impl.numChunks] =
      impl.lineStart.empty() ? 0u : static_cast<std::uint32_t>(impl.lineStart.size() - 1);

  impl.lineIndexData = data;
  impl.lineIndexSize = size;
}

} // namespace pegium::workspace
