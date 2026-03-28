#include <fuzz/WorkspaceFuzzHarness.hpp>
#include <fuzz/AdversarialLanguage.hpp>
#include <fuzz/StressLanguage.hpp>

#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AbstractReference.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace pegium::fuzz {
namespace {

struct ParseDiagnosticSnapshot {
  parser::ParseDiagnosticKind kind = parser::ParseDiagnosticKind::Deleted;
  TextOffset offset = 0;
  TextOffset begin = 0;
  TextOffset end = 0;
  std::string message;

  [[nodiscard]] bool operator==(const ParseDiagnosticSnapshot &) const = default;
};

struct DiagnosticSnapshot {
  pegium::DiagnosticSeverity severity = pegium::DiagnosticSeverity::Error;
  std::string message;
  std::string source;
  std::string code;
  TextOffset begin = 0;
  TextOffset end = 0;

  [[nodiscard]] bool operator==(const DiagnosticSnapshot &) const = default;
};

struct DocumentSnapshot {
  std::string uri;
  std::string text;
  workspace::DocumentState state = workspace::DocumentState::Changed;
  bool hasAst = false;
  bool fullMatch = false;
  bool parseRecovered = false;
  bool hasRecovered = false;
  bool fullRecovered = false;
  std::uint32_t recoveryCount = 0;
  std::uint32_t recoveryWindowsTried = 0;
  std::uint32_t strictParseRuns = 0;
  std::uint32_t recoveryAttemptRuns = 0;
  std::uint32_t recoveryEdits = 0;
  TextOffset parsedLength = 0;
  TextOffset lastVisibleCursorOffset = 0;
  TextOffset failureVisibleCursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  std::size_t referenceCount = 0;
  std::size_t resolvedReferenceCount = 0;
  std::size_t errorReferenceCount = 0;
  std::vector<ParseDiagnosticSnapshot> parseDiagnostics;
  std::vector<DiagnosticSnapshot> diagnostics;

  [[nodiscard]] bool operator==(const DocumentSnapshot &) const = default;
};

[[nodiscard]] std::string diagnostic_code_text(
    const std::optional<pegium::DiagnosticCode> &code) {
  if (!code.has_value()) {
    return {};
  }
  if (const auto *number = std::get_if<std::int64_t>(&*code);
      number != nullptr) {
    return std::to_string(*number);
  }
  return std::get<std::string>(*code);
}

[[nodiscard]] std::string hex_summary(std::string_view bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  const auto count = std::min<std::size_t>(bytes.size(), 8u);
  result.reserve(count * 2u);
  for (std::size_t index = 0; index < count; ++index) {
    const auto value = static_cast<unsigned char>(bytes[index]);
    result.push_back(kHex[value >> 4u]);
    result.push_back(kHex[value & 0x0fu]);
  }
  return result;
}

[[nodiscard]] std::string hex_string(std::string_view bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2u);
  for (const char byte : bytes) {
    const auto value = static_cast<unsigned char>(byte);
    result.push_back(kHex[value >> 4u]);
    result.push_back(kHex[value & 0x0fu]);
  }
  return result;
}

[[nodiscard]] std::string quoted_text(std::string_view text) {
  return testing::PrintToString(std::string{text});
}

[[nodiscard]] bool fuzz_trace_enabled() noexcept {
  static const bool enabled = [] {
    if (const char *value = std::getenv("PEGIUM_FUZZ_TRACE"); value != nullptr) {
      return value[0] != '\0' && value[0] != '0';
    }
    return false;
  }();
  return enabled;
}

void trace_snapshot(std::string_view label, const DocumentSnapshot &snapshot) {
  if (!fuzz_trace_enabled()) {
    return;
  }
  std::fprintf(stderr,
               "[fuzz-trace] %.*s uri=%s state=%d fullMatch=%d "
               "parseRecovered=%d fullRecovered=%d recoveryCount=%u "
               "windows=%u strictRuns=%u attemptRuns=%u edits=%u "
               "parsed=%u lastVisible=%u failureVisible=%u maxCursor=%u "
               "parseDiagnostics=%zu diagnostics=%zu refs=%zu/%zu errRefs=%zu\n",
               static_cast<int>(label.size()), label.data(), snapshot.uri.c_str(),
               static_cast<int>(snapshot.state), snapshot.fullMatch,
               snapshot.parseRecovered, snapshot.fullRecovered,
               snapshot.recoveryCount, snapshot.recoveryWindowsTried,
               snapshot.strictParseRuns, snapshot.recoveryAttemptRuns,
               snapshot.recoveryEdits, snapshot.parsedLength,
               snapshot.lastVisibleCursorOffset,
               snapshot.failureVisibleCursorOffset, snapshot.maxCursorOffset,
               snapshot.parseDiagnostics.size(), snapshot.diagnostics.size(),
               snapshot.resolvedReferenceCount, snapshot.referenceCount,
               snapshot.errorReferenceCount);
}

constexpr auto kInterestingTokens = std::to_array<std::string_view>({
    ";",        ":",      ",",       ".",        "(",      ")",
    "{",        "}",      "[",       "]",        "<",      ">",
    "=",        "!",      "&",       "|",        "\n",     " ",
    "\"",       "'",      "/*",      "*/",       "//",     "+",
    "-",        "*",      "/",       "->",       "::",     "module",
    "decl",     "extends","many",    "fallback", "choose", "bag",
    "peek",     "guard",  "path",    "doc",      "setting","tuple",
    "expr",     "legacy", "on",      "off",      "alpha",  "beta",
    "gamma",    "entity", "event",   "command",  "graph",  "node",
    "alias",    "link",   "mix",     "probe",    "pack",   "case",
    "eval",     "when",   "export",  "yes",      "no",     "fast",
    "slow",     "deep",
});

[[nodiscard]] char replacement_char(char c) noexcept {
  switch (c) {
  case ';':
    return ',';
  case ',':
    return ';';
  case ':':
    return '.';
  case '.':
    return ':';
  case '(':
    return ')';
  case ')':
    return '(';
  case '{':
    return '}';
  case '}':
    return '{';
  case '[':
    return ']';
  case ']':
    return '[';
  case '<':
    return '>';
  case '>':
    return '<';
  case '=':
    return ';';
  case '!':
    return '=';
  case '&':
    return '|';
  case '|':
    return '&';
  case '"':
    return '\'';
  case '\'':
    return '"';
  case '/':
    return '*';
  case '*':
    return '/';
  case '\n':
    return ' ';
  case ' ':
    return '\n';
  default:
    break;
  }
  if (std::islower(static_cast<unsigned char>(c)) != 0) {
    return c == 'x' ? 'a' : 'x';
  }
  if (std::isupper(static_cast<unsigned char>(c)) != 0) {
    return c == 'X' ? 'A' : 'X';
  }
  if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
    return c == '9' ? '0' : '9';
  }
  return 'x';
}

[[nodiscard]] char printable_byte(unsigned char value) noexcept {
  static constexpr std::string_view kInterestingChars =
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789_ \n;:,.=(){}/*\"'";
  if ((value & 0x03u) != 0u) {
    return kInterestingChars[value % kInterestingChars.size()];
  }
  return static_cast<char>(32 + (value % 95));
}

[[nodiscard]] std::optional<std::size_t>
find_token_offset(std::string_view text, std::string_view token,
                  bool fromEnd) noexcept {
  if (token.empty()) {
    return std::nullopt;
  }
  const auto position = fromEnd ? text.rfind(token) : text.find(token);
  if (position == std::string_view::npos) {
    return std::nullopt;
  }
  return position;
}

struct ContextualTokenMatch {
  std::size_t offset = 0;
  std::string_view token;
};

[[nodiscard]] std::optional<ContextualTokenMatch>
find_contextual_token(std::string_view text, unsigned char selector,
                      bool fromEnd) noexcept {
  for (std::size_t attempt = 0; attempt < kInterestingTokens.size();
       ++attempt) {
    const auto tokenIndex =
        (static_cast<std::size_t>(selector) + attempt) % kInterestingTokens.size();
    const auto token = kInterestingTokens[tokenIndex];
    const auto offset = find_token_offset(text, token, fromEnd);
    if (offset.has_value()) {
      return ContextualTokenMatch{.offset = *offset, .token = token};
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::string_view
alternate_contextual_token(std::string_view current,
                           unsigned char selector) noexcept {
  for (std::size_t attempt = 0; attempt < kInterestingTokens.size();
       ++attempt) {
    const auto token =
        kInterestingTokens[(static_cast<std::size_t>(selector) + attempt) %
                           kInterestingTokens.size()];
    if (token != current) {
      return token;
    }
  }
  return current;
}

void apply_contextual_token_swap_combo(std::string &text,
                                       unsigned char locationByte,
                                       unsigned char valueByte) {
  const auto focus =
      find_contextual_token(text, valueByte, (locationByte & 1u) != 0u);
  if (!focus.has_value()) {
    return;
  }

  const auto replacement =
      alternate_contextual_token(focus->token, locationByte);
  text.replace(focus->offset, focus->token.size(), replacement);

  const auto trailer = ((locationByte >> 1u) & 1u) != 0u
                           ? std::string_view{"/*"}
                           : alternate_contextual_token(
                                 replacement, static_cast<unsigned char>(
                                                  valueByte + 1u));
  const auto trailerOffset =
      std::min(text.size(), focus->offset + replacement.size());
  text.insert(trailerOffset, trailer);
}

void apply_contextual_tail_cut_combo(std::string &text,
                                     unsigned char locationByte,
                                     unsigned char valueByte) {
  const auto focus =
      find_contextual_token(text, valueByte, (locationByte & 1u) != 0u);
  if (!focus.has_value()) {
    return;
  }

  const auto inserted = ((valueByte & 1u) != 0u) ? std::string_view{"/*"}
                                                 : std::string_view{"//"};
  const auto pivot = std::min(
      text.size(),
      focus->offset + ((((locationByte >> 1u) & 1u) != 0u) ? focus->token.size()
                                                            : 0u));
  text.insert(pivot, inserted);

  const auto eraseFrom =
      std::min(text.size(), pivot + inserted.size() + (valueByte % 4u));
  if (eraseFrom < text.size()) {
    text.erase(eraseFrom);
  }
}

void apply_contextual_pair_break_combo(std::string &text,
                                       unsigned char locationByte,
                                       unsigned char valueByte) {
  const auto focus =
      find_contextual_token(text, valueByte, (locationByte & 1u) != 0u);
  if (!focus.has_value()) {
    return;
  }

  const auto replacement = focus->token == ")"   ? std::string_view{"("}
                           : focus->token == "}" ? std::string_view{"{"}
                           : focus->token == ";" ? std::string_view{","}
                                                 : std::string_view{"("};
  text.replace(focus->offset, focus->token.size(), replacement);

  const auto nearby = find_contextual_token(
      std::string_view(text).substr(
          std::min(text.size(), focus->offset + replacement.size())),
      static_cast<unsigned char>(locationByte + valueByte + 1u), false);
  if (nearby.has_value()) {
    const auto nearbyOffset =
        focus->offset + replacement.size() + nearby->offset;
    text.erase(nearbyOffset, nearby->token.size());
  }

  const auto suffix = ((locationByte >> 1u) & 1u) != 0u
                          ? std::string_view{";"}
                          : std::string_view{"/*"};
  text.insert(std::min(text.size(), focus->offset + replacement.size()), suffix);
}

void apply_contextual_cascade_combo(std::string &text,
                                    unsigned char locationByte,
                                    unsigned char valueByte) {
  const auto focus =
      find_contextual_token(text, valueByte, (locationByte & 1u) != 0u);
  if (!focus.has_value()) {
    return;
  }

  const auto replacement =
      focus->token == ")"   ? std::string_view{"("}
      : focus->token == "}" ? std::string_view{"{"}
      : focus->token == ";" ? std::string_view{","}
      : focus->token == "," ? std::string_view{";"}
      : focus->token == "." ? std::string_view{":"}
                            : alternate_contextual_token(focus->token,
                                                         locationByte);
  text.replace(focus->offset, focus->token.size(), replacement);

  const auto commentToken = ((valueByte >> 1u) & 1u) != 0u
                                ? std::string_view{"/*"}
                                : std::string_view{"//"};
  auto pivot = std::min(text.size(), focus->offset + replacement.size());
  text.insert(pivot, commentToken);
  pivot = std::min(text.size(), pivot + commentToken.size());

  const auto tail = find_contextual_token(
      std::string_view(text).substr(pivot),
      static_cast<unsigned char>(locationByte + valueByte + 3u), false);
  if (tail.has_value()) {
    const auto eraseOffset = std::min(text.size(), pivot + tail->offset);
    if (eraseOffset < text.size()) {
      text.erase(eraseOffset,
                 std::min<std::size_t>(tail->token.size(),
                                       text.size() - eraseOffset));
    }
  }

  const auto cutOffset = std::min(text.size(), pivot + (valueByte % 6u));
  if (cutOffset < text.size()) {
    text.erase(cutOffset);
  }
}

void apply_contextual_window_splice_combo(std::string &text,
                                          unsigned char locationByte,
                                          unsigned char valueByte) {
  const auto focus =
      find_contextual_token(text, valueByte, (locationByte & 1u) != 0u);
  if (!focus.has_value()) {
    return;
  }

  const auto leftSlack = static_cast<std::size_t>(locationByte % 10u);
  const auto rightSlack = static_cast<std::size_t>(1u + (valueByte % 12u));
  const auto windowBegin = focus->offset > leftSlack ? focus->offset - leftSlack : 0u;
  const auto windowEnd = std::min(text.size(),
                                  focus->offset + focus->token.size() + rightSlack);
  if (windowEnd <= windowBegin) {
    return;
  }

  const auto snippetLength = std::min<std::size_t>(
      windowEnd - windowBegin, 1u + ((locationByte + valueByte) % 14u));
  if (snippetLength == 0u) {
    return;
  }

  const auto snippet = text.substr(windowBegin, snippetLength);
  auto insertOffset = std::min(text.size(), focus->offset + focus->token.size());
  const auto prefix = ((locationByte >> 1u) & 1u) != 0u
                          ? std::string_view{"/*"}
                          : std::string_view{"("};
  text.insert(insertOffset, prefix);
  insertOffset = std::min(text.size(), insertOffset + prefix.size());
  text.insert(insertOffset, snippet);

  const auto tail = find_contextual_token(
      std::string_view(text).substr(
          std::min(text.size(), insertOffset + snippet.size())),
      static_cast<unsigned char>(valueByte + 5u), false);
  if (tail.has_value()) {
    const auto eraseOffset = std::min(text.size(),
                                      insertOffset + snippet.size() + tail->offset);
    if (eraseOffset < text.size()) {
      text.erase(eraseOffset,
                 std::min<std::size_t>(tail->token.size(),
                                       text.size() - eraseOffset));
    }
  }
}

void apply_contextual_multi_burst_combo(std::string &text,
                                        unsigned char locationByte,
                                        unsigned char valueByte) {
  apply_contextual_token_swap_combo(text, locationByte, valueByte);
  apply_contextual_pair_break_combo(
      text, static_cast<unsigned char>(locationByte + 3u),
      static_cast<unsigned char>(valueByte + 5u));
  apply_contextual_window_splice_combo(
      text, static_cast<unsigned char>(locationByte + 7u),
      static_cast<unsigned char>(valueByte + 11u));
}

[[nodiscard]] std::vector<bool> protected_offsets(std::string_view text) {
  std::vector<bool> protectedBytes(text.size(), false);

  for (std::size_t i = 0; i < text.size();) {
    if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
      const auto begin = i;
      i += 2;
      while (i < text.size() && text[i] != '\n') {
        ++i;
      }
      std::fill(protectedBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                protectedBytes.begin() + static_cast<std::ptrdiff_t>(i), true);
      continue;
    }
    if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
      const auto begin = i;
      i += 2;
      while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
        ++i;
      }
      if (i + 1 < text.size()) {
        i += 2;
      }
      std::fill(protectedBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                protectedBytes.begin() + static_cast<std::ptrdiff_t>(i), true);
      continue;
    }
    if (text[i] == '"' || text[i] == '\'') {
      const auto quote = text[i];
      const auto begin = i++;
      while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
          i += 2;
          continue;
        }
        if (text[i] == quote) {
          ++i;
          break;
        }
        ++i;
      }
      std::fill(protectedBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                protectedBytes.begin() + static_cast<std::ptrdiff_t>(i), true);
      continue;
    }
    ++i;
  }

  return protectedBytes;
}

[[nodiscard]] std::vector<std::size_t>
editable_offsets(std::string_view text, const std::vector<bool> &protectedBytes,
                 bool includeEnd) {
  std::vector<std::size_t> offsets;
  offsets.reserve(text.size() + (includeEnd ? 1u : 0u));
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (!protectedBytes[index]) {
      offsets.push_back(index);
    }
  }
  if (includeEnd) {
    offsets.push_back(text.size());
  }
  return offsets;
}

[[nodiscard]] std::vector<std::size_t>
transpose_offsets(std::string_view text, const std::vector<bool> &protectedBytes) {
  std::vector<std::size_t> offsets;
  if (text.size() < 2u) {
    return offsets;
  }
  offsets.reserve(text.size() - 1u);
  for (std::size_t index = 0; index + 1u < text.size(); ++index) {
    if (!protectedBytes[index] && !protectedBytes[index + 1u]) {
      offsets.push_back(index);
    }
  }
  return offsets;
}

[[nodiscard]] std::size_t choose_offset(std::span<const std::size_t> offsets,
                                        unsigned char selector,
                                        std::size_t fallback) {
  if (offsets.empty()) {
    return fallback;
  }
  return offsets[selector % offsets.size()];
}

void mutate_text(std::string &text, std::string_view mutationProgram) {
  constexpr std::size_t kMaxOperations = 12u;
  constexpr std::size_t kMaxSliceLength = 8u;
  constexpr std::size_t kMaxDocumentSize = 128u * 1024u;

  const auto operationCount = std::min<std::size_t>(
      kMaxOperations, (mutationProgram.size() + 2u) / 3u);
  std::size_t cursor = 0;
  for (std::size_t index = 0; index < operationCount; ++index) {
    const auto opcode = cursor < mutationProgram.size()
                            ? static_cast<unsigned char>(mutationProgram[cursor++])
                            : 0u;
    const auto locationByte = cursor < mutationProgram.size()
                                  ? static_cast<unsigned char>(mutationProgram[cursor++])
                                  : 0u;
    const auto valueByte = cursor < mutationProgram.size()
                               ? static_cast<unsigned char>(mutationProgram[cursor++])
                               : 0u;

    const auto protectedBytes = protected_offsets(text);
    switch (opcode % 15u) {
    case 0u: {
      if (text.empty()) {
        break;
      }
      const auto offsets = editable_offsets(text, protectedBytes, false);
      if (offsets.empty()) {
        break;
      }
      const auto offset = choose_offset(offsets, locationByte, text.size() - 1u);
      text.erase(text.begin() + static_cast<std::ptrdiff_t>(offset));
      break;
    }
    case 1u: {
      const auto offsets = editable_offsets(text, protectedBytes, true);
      const auto offset = choose_offset(offsets, locationByte, text.size());
      const auto inserted =
          offset < text.size() && (valueByte & 1u) != 0u
              ? text[offset]
              : printable_byte(valueByte);
      text.insert(text.begin() + static_cast<std::ptrdiff_t>(offset), inserted);
      break;
    }
    case 2u: {
      if (text.empty()) {
        break;
      }
      const auto offsets = editable_offsets(text, protectedBytes, false);
      if (offsets.empty()) {
        break;
      }
      const auto offset = choose_offset(offsets, locationByte, text.size() - 1u);
      text[offset] = replacement_char(text[offset]);
      break;
    }
    case 3u: {
      const auto offsets = transpose_offsets(text, protectedBytes);
      if (offsets.empty()) {
        break;
      }
      const auto offset = choose_offset(offsets, locationByte, 0u);
      std::swap(text[offset], text[offset + 1u]);
      break;
    }
    case 4u: {
      if (text.empty()) {
        break;
      }
      const auto count =
          std::min<std::size_t>(text.size(), 1u + (valueByte % kMaxSliceLength));
      if ((locationByte & 1u) == 0u) {
        text.erase(0u, count);
      } else {
        text.erase(text.size() - count, count);
      }
      break;
    }
    case 5u: {
      if (text.empty()) {
        text.push_back(printable_byte(valueByte));
        break;
      }
      const auto start = static_cast<std::size_t>(locationByte) % text.size();
      const auto count =
          std::min<std::size_t>(text.size() - start,
                                1u + ((valueByte >> 1u) % kMaxSliceLength));
      const auto snippet = text.substr(start, count);
      if ((valueByte & 1u) == 0u) {
        text.insert(0u, snippet);
      } else {
        text.append(snippet);
      }
      break;
    }
    case 6u: {
      const auto token = kInterestingTokens[valueByte % kInterestingTokens.size()];
      const auto offset =
          find_token_offset(text, token, (locationByte & 1u) != 0u);
      if (!offset.has_value()) {
        break;
      }
      text.erase(*offset, token.size());
      break;
    }
    case 7u: {
      const auto token = kInterestingTokens[valueByte % kInterestingTokens.size()];
      const auto offset =
          find_token_offset(text, token, (locationByte & 1u) != 0u);
      if (offset.has_value()) {
        const auto insertOffset =
            *offset + (((locationByte >> 1u) & 1u) != 0u ? token.size() : 0u);
        text.insert(insertOffset, token);
        break;
      }
      if ((locationByte & 1u) == 0u) {
        text.insert(0u, token);
      } else {
        text.append(token);
      }
      break;
    }
    case 8u: {
      const auto fromToken =
          kInterestingTokens[valueByte % kInterestingTokens.size()];
      auto toToken = kInterestingTokens[locationByte % kInterestingTokens.size()];
      if (fromToken == toToken) {
        toToken = kInterestingTokens[(locationByte + 1u) % kInterestingTokens.size()];
      }
      const auto offset = find_token_offset(text, fromToken, false);
      if (!offset.has_value()) {
        break;
      }
      text.replace(*offset, fromToken.size(), toToken);
      break;
    }
    case 9u:
      apply_contextual_token_swap_combo(text, locationByte, valueByte);
      break;
    case 10u:
      apply_contextual_tail_cut_combo(text, locationByte, valueByte);
      break;
    case 11u:
      apply_contextual_pair_break_combo(text, locationByte, valueByte);
      break;
    case 12u:
      apply_contextual_cascade_combo(text, locationByte, valueByte);
      break;
    case 13u:
      apply_contextual_window_splice_combo(text, locationByte, valueByte);
      break;
    case 14u:
      apply_contextual_multi_burst_combo(text, locationByte, valueByte);
      break;
    }

    if (text.size() > kMaxDocumentSize) {
      text.erase(kMaxDocumentSize);
    }
  }
}

[[nodiscard]] DocumentSnapshot snapshot_document(
    const workspace::Document &document) {
  using Clock = std::chrono::steady_clock;
  const auto snapshotStart = Clock::now();
  const auto text = std::string(document.textDocument().getText());
  const auto textSize = static_cast<TextOffset>(text.size());

  EXPECT_EQ(document.state, workspace::DocumentState::Validated);
  EXPECT_EQ(document.references.size(), document.parseResult.references.size());
  EXPECT_LE(document.parsedLength(), textSize);
  EXPECT_LE(document.parseResult.lastVisibleCursorOffset, textSize);
  EXPECT_LE(document.parseResult.failureVisibleCursorOffset, textSize);
  EXPECT_LE(document.parseResult.maxCursorOffset, textSize);

  if (document.parseResult.cst != nullptr) {
    const auto cstTextStart = Clock::now();
    const auto cstText = document.parseResult.cst->getText();
    const auto cstTextElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - cstTextStart);
    EXPECT_EQ(cstText, text);
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] cstText uri=%s ms=%lld size=%zu\n",
                   document.uri.c_str(),
                   static_cast<long long>(cstTextElapsed.count()), cstText.size());
    }
  }

  bool hasSyntaxDiagnostic = false;
  std::vector<ParseDiagnosticSnapshot> parseDiagnostics;
  parseDiagnostics.reserve(document.parseResult.parseDiagnostics.size());
  for (const auto &diagnostic : document.parseResult.parseDiagnostics) {
    EXPECT_LE(diagnostic.beginOffset, diagnostic.endOffset);
    EXPECT_LE(diagnostic.beginOffset, textSize);
    EXPECT_LE(diagnostic.endOffset, textSize);
    EXPECT_LE(diagnostic.offset, textSize);
    hasSyntaxDiagnostic = hasSyntaxDiagnostic || diagnostic.isSyntax();
    parseDiagnostics.push_back(ParseDiagnosticSnapshot{
        .kind = diagnostic.kind,
        .offset = diagnostic.offset,
        .begin = diagnostic.beginOffset,
        .end = diagnostic.endOffset,
        .message = diagnostic.message,
    });
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] parseDiagnostic uri=%s kind=%d offset=%u "
                   "range=%u-%u message=%s\n",
                   document.uri.c_str(), static_cast<int>(diagnostic.kind),
                   diagnostic.offset, diagnostic.beginOffset,
                   diagnostic.endOffset, diagnostic.message.c_str());
    }
  }
  std::ranges::sort(parseDiagnostics, [](const auto &left, const auto &right) {
    return std::tie(left.offset, left.begin, left.end, left.kind, left.message) <
           std::tie(right.offset, right.begin, right.end, right.kind,
                    right.message);
  });

  std::vector<DiagnosticSnapshot> diagnostics;
  diagnostics.reserve(document.diagnostics.size());
  for (const auto &diagnostic : document.diagnostics) {
    EXPECT_LE(diagnostic.begin, diagnostic.end);
    EXPECT_LE(diagnostic.begin, textSize);
    EXPECT_LE(diagnostic.end, textSize);
    diagnostics.push_back(DiagnosticSnapshot{
        .severity = diagnostic.severity,
        .message = diagnostic.message,
        .source = diagnostic.source,
        .code = diagnostic_code_text(diagnostic.code),
        .begin = diagnostic.begin,
        .end = diagnostic.end,
    });
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] diagnostic uri=%s severity=%d range=%u-%u "
                   "source=%s code=%s message=%s\n",
                   document.uri.c_str(), static_cast<int>(diagnostic.severity),
                   diagnostic.begin, diagnostic.end, diagnostic.source.c_str(),
                   diagnostic_code_text(diagnostic.code).c_str(),
                   diagnostic.message.c_str());
    }
  }
  std::ranges::sort(diagnostics, [](const auto &left, const auto &right) {
    return std::tie(left.begin, left.end, left.severity, left.source,
                    left.message, left.code) <
           std::tie(right.begin, right.end, right.severity, right.source,
                    right.message, right.code);
  });

  if (!document.parseResult.fullMatch) {
    EXPECT_TRUE(hasSyntaxDiagnostic);
  }
  if (document.parseResult.recoveryReport.fullRecovered) {
    EXPECT_TRUE(document.parseResult.recoveryReport.hasRecovered);
    EXPECT_GT(document.parseResult.recoveryReport.recoveryCount, 0u);
  }

  std::size_t resolvedReferenceCount = 0;
  std::size_t errorReferenceCount = 0;
  for (const auto &handle : document.references) {
    const auto *reference = handle.getConst();
    EXPECT_NE(reference, nullptr);
    EXPECT_NE(reference->state(), ReferenceState::Unresolved);
    if (reference->state() == ReferenceState::Resolved) {
      ++resolvedReferenceCount;
    }
    if (reference->hasError()) {
      ++errorReferenceCount;
    }
  }
  if (errorReferenceCount > 0u) {
    EXPECT_FALSE(document.diagnostics.empty());
  }

  auto snapshot = DocumentSnapshot{
      .uri = document.uri,
      .text = text,
      .state = document.state,
      .hasAst = document.hasAst(),
      .fullMatch = document.parseResult.fullMatch,
      .parseRecovered = document.parseRecovered(),
      .hasRecovered = document.parseResult.recoveryReport.hasRecovered,
      .fullRecovered = document.parseResult.recoveryReport.fullRecovered,
      .recoveryCount = document.parseResult.recoveryReport.recoveryCount,
      .recoveryWindowsTried =
          document.parseResult.recoveryReport.recoveryWindowsTried,
      .strictParseRuns = document.parseResult.recoveryReport.strictParseRuns,
      .recoveryAttemptRuns =
          document.parseResult.recoveryReport.recoveryAttemptRuns,
      .recoveryEdits = document.parseResult.recoveryReport.recoveryEdits,
      .parsedLength = document.parseResult.parsedLength,
      .lastVisibleCursorOffset =
          document.parseResult.lastVisibleCursorOffset,
      .failureVisibleCursorOffset =
          document.parseResult.failureVisibleCursorOffset,
      .maxCursorOffset = document.parseResult.maxCursorOffset,
      .referenceCount = document.references.size(),
      .resolvedReferenceCount = resolvedReferenceCount,
      .errorReferenceCount = errorReferenceCount,
      .parseDiagnostics = std::move(parseDiagnostics),
      .diagnostics = std::move(diagnostics),
  };

  if (fuzz_trace_enabled()) {
    const auto snapshotElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - snapshotStart);
    std::fprintf(stderr, "[fuzz-trace] snapshotDocument uri=%s ms=%lld\n",
                 snapshot.uri.c_str(),
                 static_cast<long long>(snapshotElapsed.count()));
  }

  return snapshot;
}

class WorkspaceScenarioRunner {
public:
  explicit WorkspaceScenarioRunner(const WorkspaceScenarioSpec &scenario)
      : _scenario(scenario), _shared(test::make_empty_shared_services()),
        _versions(scenario.documents.size(), 0),
        _currentTexts(scenario.documents.size()) {
    for (std::size_t index = 0; index < scenario.documents.size(); ++index) {
      _currentTexts[index] = scenario.documents[index].text;
    }
  }

  void initialize() {
    ASSERT_NE(_shared, nullptr);
    pegium::installDefaultSharedCoreServices(*_shared);
    installDefaultSharedLspServices(*_shared);
    test::initialize_shared_workspace_for_tests(*_shared);
    register_required_languages();
  }

  [[nodiscard]] std::vector<DocumentSnapshot> build_initial() {
    std::vector<std::size_t> changedIndices;
    changedIndices.reserve(_scenario.documents.size());
    for (std::size_t index = 0; index < _scenario.documents.size(); ++index) {
      changedIndices.push_back(index);
    }
    apply_changes(changedIndices);
    return snapshot_all();
  }

  [[nodiscard]] std::vector<DocumentSnapshot>
  update_document(std::size_t targetIndex, std::string text) {
    EXPECT_LT(targetIndex, _scenario.documents.size());
    if (targetIndex >= _scenario.documents.size()) {
      return {};
    }
    if (_currentTexts[targetIndex] == text) {
      return snapshot_all();
    }
    _currentTexts[targetIndex] = std::move(text);
    apply_changes(std::span<const std::size_t>(&targetIndex, 1u));
    return snapshot_all();
  }

  [[nodiscard]] std::vector<DocumentSnapshot> current_snapshots() const {
    return snapshot_all();
  }

private:
  void register_required_languages() {
    bool needsStress = false;
    bool needsAdversarial = false;
    for (const auto &document : _scenario.documents) {
      needsStress = needsStress || document.languageId == "stress-language";
      needsAdversarial =
          needsAdversarial || document.languageId == "adversarial-language";
    }
    if (needsStress) {
      ASSERT_TRUE(stress::register_stress_language_services(*_shared));
    }
    if (needsAdversarial) {
      ASSERT_TRUE(
          adversarial::register_adversarial_language_services(*_shared));
    }
  }

  void apply_changes(std::span<const std::size_t> changedIndices) {
    using Clock = std::chrono::steady_clock;
    const auto applyStart = Clock::now();
    auto documents = test::text_documents(*_shared);
    ASSERT_NE(documents, nullptr);

    std::vector<workspace::DocumentId> changedDocumentIds;
    changedDocumentIds.reserve(changedIndices.size());
    for (const auto index : changedIndices) {
      ASSERT_LT(index, _scenario.documents.size());
      const auto &document = _scenario.documents[index];
      ++_versions[index];
      auto textDocument = test::set_text_document(
          *documents, document.uri, document.languageId, _currentTexts[index],
          _versions[index]);
      ASSERT_NE(textDocument, nullptr);
      changedDocumentIds.push_back(
          _shared->workspace.documents->getOrCreateDocumentId(textDocument->uri()));
    }

    (void)_shared->workspace.documentBuilder->update(changedDocumentIds, {});

    const auto updateElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - applyStart);

    const auto waitStart = Clock::now();
    for (const auto &document : _scenario.documents) {
      const auto documentId =
          _shared->workspace.documents->getDocumentId(document.uri);
      ASSERT_NE(documentId, workspace::InvalidDocumentId);
      (void)_shared->workspace.documentBuilder->waitUntil(
          workspace::DocumentState::Validated, documentId);
    }

    if (fuzz_trace_enabled()) {
      const auto waitElapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              Clock::now() - waitStart);
      std::fprintf(stderr,
                   "[fuzz-trace] applyChanges scenario=%s changed=%zu "
                   "updateMs=%lld waitMs=%lld\n",
                   _scenario.name.c_str(), changedIndices.size(),
                   static_cast<long long>(updateElapsed.count()),
                   static_cast<long long>(waitElapsed.count()));
    }
  }

  [[nodiscard]] std::vector<DocumentSnapshot> snapshot_all() const {
    using Clock = std::chrono::steady_clock;
    const auto snapshotStart = Clock::now();
    std::vector<DocumentSnapshot> snapshots;
    snapshots.reserve(_scenario.documents.size());
    for (std::size_t index = 0; index < _scenario.documents.size(); ++index) {
      const auto &documentSpec = _scenario.documents[index];
      auto document =
          _shared->workspace.documents->getDocument(documentSpec.uri);
      EXPECT_NE(document, nullptr) << documentSpec.uri;
      if (document == nullptr) {
        continue;
      }
      EXPECT_EQ(document->textDocument().getText(), _currentTexts[index]);
      snapshots.push_back(snapshot_document(*document));
    }
    if (fuzz_trace_enabled()) {
      const auto snapshotElapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              Clock::now() - snapshotStart);
      std::fprintf(stderr, "[fuzz-trace] snapshotAll scenario=%s ms=%lld\n",
                   _scenario.name.c_str(),
                   static_cast<long long>(snapshotElapsed.count()));
    }
    return snapshots;
  }

  const WorkspaceScenarioSpec &_scenario;
  std::unique_ptr<SharedServices> _shared;
  std::vector<std::int64_t> _versions;
  std::vector<std::string> _currentTexts;
};

struct CachedWorkspaceRoundTripState {
  std::unique_ptr<WorkspaceScenarioRunner> runner;
  std::vector<DocumentSnapshot> baselineSnapshots;
};

[[nodiscard]] CachedWorkspaceRoundTripState &
cached_workspace_round_trip_state(const WorkspaceScenarioSpec &scenario) {
  static std::unordered_map<const WorkspaceScenarioSpec *,
                            CachedWorkspaceRoundTripState>
      cache;
  return cache[std::addressof(scenario)];
}

void reset_cached_workspace_round_trip_state(
    const WorkspaceScenarioSpec &scenario,
    CachedWorkspaceRoundTripState &state) {
  state = {};
  state.runner = std::make_unique<WorkspaceScenarioRunner>(scenario);
  state.runner->initialize();
  state.baselineSnapshots = state.runner->build_initial();
}

[[nodiscard]] CachedWorkspaceRoundTripState &
ensure_cached_workspace_round_trip_state(const WorkspaceScenarioSpec &scenario) {
  auto &state = cached_workspace_round_trip_state(scenario);
  if (!state.runner) {
    reset_cached_workspace_round_trip_state(scenario, state);
    return state;
  }
  if (state.runner->current_snapshots() != state.baselineSnapshots) {
    reset_cached_workspace_round_trip_state(scenario, state);
  }
  return state;
}

[[nodiscard]] WorkspaceScenarioSpec make_single_document_scenario(
    std::string name, std::string fileName, std::string languageId,
    std::string text) {
  return WorkspaceScenarioSpec{
      .name = std::move(name),
      .documents = {ScenarioDocumentSpec{
          .uri = test::make_file_uri(fileName),
          .languageId = std::move(languageId),
          .text = std::move(text),
      }},
  };
}

[[nodiscard]] WorkspaceScenarioSpec make_workspace_scenario(
    std::string name, std::vector<ScenarioDocumentSpec> documents) {
  return WorkspaceScenarioSpec{
      .name = std::move(name),
      .documents = std::move(documents),
  };
}

} // namespace

const std::vector<WorkspaceScenarioSpec> &stress_single_document_scenarios() {
  static const auto scenarios = [] {
    return std::vector<WorkspaceScenarioSpec>{
        make_single_document_scenario(
            "stress/coverage.stress", "fuzz-coverage.stress",
            "stress-language",
            "module Demo\n"
            "// line comment\n"
            "/* block comment */\n"
            "export decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Derived extends Base {\n"
            "  many items: Base;\n"
            "  child: Derived;\n"
            "}\n"
            "use Base, Derived fallback Base;\n"
            "export choose event;\n"
            "bag gamma alpha beta;\n"
            "peek alpha;\n"
            "guard token;\n"
            "path demo.inner.Leaf;\n"
            "doc \"line\\nvalue\";\n"
            "setting safe = on;\n"
            "tuple( one, two, three );\n"
            "expr 1 + 2 * (3 + 4);\n"
            "expr Derived(Base, 2 + 3);\n"
            "expr off;\n"
            "legacy 7 + 8 - 9 * 10;\n"),
        make_single_document_scenario(
            "stress/recovery-dense.stress", "fuzz-recovery-dense.stress",
            "stress-language",
            "module Recovery\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Derived extends Base {\n"
            "  many items: Base\n"
            "  child: Missing;\n"
            "}\n"
            "use Base, Derived fallback Missing;\n"
            "choose entity\n"
            "bag beta alpha;\n"
            "peek delta;\n"
            "guard =;\n"
            "path broken.;\n"
            "setting unsafe = ;\n"
            "tuple(one,\n"
            "  two, three);\n"
            "expr 1 + * 2;\n"
            "legacy 3 + ;\n"
            "doc \"still closed\";\n"),
        make_single_document_scenario(
            "stress/link-and-validation.stress",
            "fuzz-link-and-validation.stress", "stress-language",
            "module Validation\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Empty {\n"
            "}\n"
            "decl Loop extends Loop {\n"
            "  next: Loop;\n"
            "}\n"
            "decl Consumer {\n"
            "  ref: MissingType;\n"
            "}\n"
            "use Base, MissingType fallback MissingType;\n"
            "choose command;\n"
            "bag alpha gamma beta;\n"
            "peek beta;\n"
            "guard valid;\n"
            "path one.two.three;\n"
            "doc 'single quoted';\n"
            "setting unsafe = off;\n"
            "tuple(one, one, two);\n"
            "expr MissingType + 1;\n"
            "legacy Loop + MissingType;\n"),
        make_single_document_scenario(
            "stress/truncated-eof.stress", "fuzz-truncated-eof.stress",
            "stress-language",
            "module Cut\n"
            "decl Alpha {\n"
            "  ref: Alpha;\n"
            "}\n"
            "use Alpha fallback Alpha;\n"
            "bag alpha beta gamma;\n"
            "peek gamma;\n"
            "guard trailing;\n"
            "path cut.inner.Path;\n"
            "tuple(one, two, three);\n"
            "expr (Alpha(1, 2) + 3\n"
            "legacy 4 *\n"),
        make_single_document_scenario(
            "stress/legacy-eof-focus.stress",
            "fuzz-legacy-eof-focus.stress", "stress-language",
            "module LegacyFocus\n"
            "legacy 4 *\n"),
        make_single_document_scenario(
            "stress/setting-eof-focus.stress",
            "fuzz-setting-eof-focus.stress", "stress-language",
            "module SettingFocus\n"
            "setting safe =\n"),
        make_single_document_scenario(
            "stress/unclosed-string-eof.stress",
            "fuzz-unclosed-string-eof.stress", "stress-language",
            "module Trivia\n"
            "decl Alpha {\n"
            "  ref: Alpha;\n"
            "}\n"
            "use Alpha fallback Alpha;\n"
            "bag beta gamma alpha;\n"
            "peek beta;\n"
            "guard guardName;\n"
            "tuple(one, two, three);\n"
            "doc \"unfinished\n"),
        make_single_document_scenario(
            "stress/unclosed-comment-eof.stress",
            "fuzz-unclosed-comment-eof.stress", "stress-language",
            "module Commented\n"
            "decl Alpha {\n"
            "  ref: Alpha;\n"
            "}\n"
            "use Alpha fallback Alpha;\n"
            "bag alpha gamma beta;\n"
            "peek gamma;\n"
            "guard commentGuard;\n"
            "/* unfinished comment\n"),
        make_single_document_scenario(
            "stress/trivia-and-case.stress",
            "fuzz-trivia-and-case.stress", "stress-language",
            "module TriviaCase\n"
            "/* lead comment */\n"
            "export decl Root {\n"
            "  self: Root;\n"
            "}\n"
            "decl Leaf extends Root {\n"
            "  many items: Root;\n"
            "  child: Leaf;\n"
            "}\n"
            "// comment between statements\n"
            "use Root, Leaf fallback Root;\n"
            "bag gamma beta alpha;\n"
            "peek alpha;\n"
            "guard guardToken;\n"
            "path demo.branch.leaf;\n"
            "doc \"quote \\\" slash \\\\ line\\nnext\";\n"
            "setting safe = ON;\n"
            "tuple( one , two , three );\n"
            "expr Leaf(Root, (1 + 2) * 3);\n"
            "legacy (1 + 2) * (3 - 4) + 5;\n"),
        make_single_document_scenario(
            "stress/missing-brace-before-statement.stress",
            "fuzz-missing-brace-before-statement.stress", "stress-language",
            "module MissingBrace\n"
            "decl Base {\n"
            "  self: Base;\n"
            "decl Child extends Base {\n"
            "  many items: Base;\n"
            "  child: Child;\n"
            "}\n"
            "use Base, Child fallback Base;\n"
            "bag alpha beta gamma;\n"
            "peek beta;\n"
            "guard guardToken;\n"
            "tuple(one, two, three);\n"
            "expr Child(Base, 1 + 2);\n"),
        make_single_document_scenario(
            "stress/punctuation-chaos.stress",
            "fuzz-punctuation-chaos.stress", "stress-language",
            "module Chaos\n"
            "decl Base {\n"
            "  self:: Base;\n"
            "}\n"
            "decl Child extends Base {\n"
            "  many items: Base;\n"
            "  child: Base;;\n"
            "}\n"
            "use Base,, Child fallback ;\n"
            "bag alpha alpha beta;\n"
            "peek alpha beta;\n"
            "guard ==;\n"
            "path one..two;\n"
            "tuple(one, two, three,);\n"
            "expr Base(1 2);\n"
            "legacy (1 + 2));\n"),
        make_single_document_scenario(
            "stress/call-and-group-gaps.stress",
            "fuzz-call-and-group-gaps.stress", "stress-language",
            "module Calls\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Child extends Base {\n"
            "  many items: Base;\n"
            "  child: Child;\n"
            "}\n"
            "use Base, Child fallback Child;\n"
            "bag beta alpha gamma;\n"
            "peek gamma;\n"
            "guard callGuard;\n"
            "tuple(one, two, three);\n"
            "expr Child(Base(1 + 2, Child(3, 4), (5 + 6);\n"
            "legacy ((1 + 2) * (3 - 4);\n"),
        make_single_document_scenario(
            "stress/entry-fragment.stress", "fuzz-entry-fragment.stress",
            "stress-language",
            "module\n"
            "decl {\n"
            "use ;\n"
            "bag alpha;\n"
            "peek ;\n"
            "expr ;\n"),
        make_single_document_scenario(
            "stress/operator-fragment.stress", "fuzz-operator-fragment.stress",
            "stress-language",
            "module Ops\n"
            "expr 1 + ;\n"
            "expr (1 + 2 *);\n"
            "legacy 3 * ;\n"
            "legacy (4 + );\n"),
        make_single_document_scenario(
            "stress/legacy-operator-chaos.stress",
            "fuzz-legacy-operator-chaos.stress", "stress-language",
            "module LegacyOps\n"
            "legacy +;\n"
            "legacy a + + b;\n"
            "legacy a - * b;\n"
            "legacy 1 * / 2;\n"
            "legacy 1 + - 2;\n"
            "legacy a b;\n"),
        make_single_document_scenario(
            "stress/choice-and-bag-fragment.stress",
            "fuzz-choice-and-bag-fragment.stress", "stress-language",
            "module Choice\n"
            "choose ;\n"
            "choose entity event;\n"
            "bag alpha alpha;\n"
            "bag gamma;\n"
            "peek ;\n"
            "peek alpha beta;\n"),
        make_single_document_scenario(
            "stress/path-and-use-fragment.stress",
            "fuzz-path-and-use-fragment.stress", "stress-language",
            "module Links\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "use Base, fallback ;\n"
            "use , Base;\n"
            "path one.;\n"
            "path .two;\n"),
        make_single_document_scenario(
            "stress/doc-and-setting-fragment.stress",
            "fuzz-doc-and-setting-fragment.stress", "stress-language",
            "module TriviaBits\n"
            "doc 'unterminated\n"
            "doc \"closed\";\n"
            "setting safe = ;\n"
            "setting = on;\n"),
        make_single_document_scenario(
            "stress/tuple-and-call-fragment.stress",
            "fuzz-tuple-and-call-fragment.stress", "stress-language",
            "module TupleCalls\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "tuple(one,,);\n"
            "tuple(, one);\n"
            "expr Ref(,);\n"
            "expr Ref((1 + 2), Ref(3, ),);\n"),
        make_single_document_scenario(
            "stress/comment-operator-interleave.stress",
            "fuzz-comment-operator-interleave.stress", "stress-language",
            "module CommentOps\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "legacy 1 /* gap */ + ;\n"
            "legacy Ref // trailing operator context\n"
            "  * ;\n"
            "expr Ref(1 /* hole */, 2 + );\n"),
        make_single_document_scenario(
            "stress/use-bag-tuple-structure.stress",
            "fuzz-use-bag-tuple-structure.stress", "stress-language",
            "module Structure\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Child extends Base {\n"
            "  many items: Base;\n"
            "  child: Child;\n"
            "}\n"
            "use Base Child, fallback Child\n"
            "use Base, Child fallback\n"
            "bag alpha gamma;\n"
            "bag beta alpha\n"
            "peek alpha;\n"
            "tuple(one two, three);\n"
            "tuple(one, two three);\n"
            "expr Child(Base /* gap */, Child(1, 2 + ));\n"
            "legacy Base /* mid */ + /* tail */\n"),
        make_single_document_scenario(
            "stress/commented-call-and-choice-eof.stress",
            "fuzz-commented-call-and-choice-eof.stress", "stress-language",
            "module CommentCut\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "choose event/* tail */\n"
            "expr Ref(1 /* inner */, (2 + 3) /* mid */, Ref(4, 5 /* cut */)\n"
            "legacy 1 /* gap */ + /* hole */\n"),
        make_single_document_scenario(
            "stress/repeated-delimiter-loss.stress",
            "fuzz-repeated-delimiter-loss.stress", "stress-language",
            "module Delims\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "use Base,, Base fallback Base\n"
            "tuple(one,, two,,)\n"
            "bag alpha beta\n"
            "peek gamma alpha;\n"
            "path one..two..three;\n"
            "setting safe =\n"
            "legacy 1 + 2 *\n"),
        make_single_document_scenario(
            "stress/nested-delimiter-comment-coverage.stress",
            "fuzz-nested-delimiter-comment-coverage.stress", "stress-language",
            "module NestedCoverage\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "use Ref, Ref fallback Ref;\n"
            "bag alpha beta gamma;\n"
            "path nested.inner.tail;\n"
            "tuple(one, Ref(1, 2), three);\n"
            "expr Ref(Ref(1, 2), /* mid */ Ref(3, 4), (5 + 6));\n"
            "legacy (1 + 2) * (3 - 4) + Ref;\n"),
        make_single_document_scenario(
            "stress/local-skip-trivia-chaos.stress",
            "fuzz-local-skip-trivia-chaos.stress", "stress-language",
            "module LocalSkip\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "tuple(\tone,\ttwo,\tthree\t);\n"
            "tuple(one,\n"
            "  two,\n"
            "  three);\n"
            "tuple(one, /* gap */ two);\n"
            "expr Ref(tuple(one, two), Ref(1, 2 + ));\n"
            "legacy (1 + 2\n"),
        make_single_document_scenario(
            "stress/comment-delimiter-tail-cascade.stress",
            "fuzz-comment-delimiter-tail-cascade.stress", "stress-language",
            "module CascadeCombo\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "decl Child extends Ref {\n"
            "  child: Child;\n"
            "}\n"
            "use Ref, Child fallback Ref;\n"
            "bag alpha beta gamma;\n"
            "tuple(one, Child(Ref, 2), three);\n"
            "expr Child(Ref /* mid */, Child(1, 2), (3 + 4));\n"
            "path Ref.Child.Inner;\n"
            "legacy Child + Ref - 3 * 4;\n"),
        make_single_document_scenario(
            "stress/predicate-and-unordered-chaos.stress",
            "fuzz-predicate-and-unordered-chaos.stress", "stress-language",
            "module Predicates\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "bag alpha gamma alpha;\n"
            "bag beta /* hole */ gamma;\n"
            "peek alpha beta;\n"
            "peek /* gap */ gamma;\n"
            "guard token=;\n"
            "guard token /* tail */ ;\n"
            "expr Ref + Ref;\n"
            "legacy Ref /* gap */ - ;\n"),
        make_single_document_scenario(
            "stress/string-escape-chaos.stress",
            "fuzz-string-escape-chaos.stress", "stress-language",
            "module Strings\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "doc \"quote \\\" slash \\\\ tab \\t end\";\n"
            "doc 'single \\' escape';\n"
            "doc \"trailing slash \\\n"
            "expr Ref(1, 2 + 3);\n"
            "legacy Ref + \n"),
        make_single_document_scenario(
            "stress/qualified-name-and-field-chaos.stress",
            "fuzz-qualified-name-and-field-chaos.stress", "stress-language",
            "module Links2\n"
            "decl Base {\n"
            "  self Base;\n"
            "  many items: ;\n"
            "}\n"
            "decl Child extends Base. {\n"
            "  child: Base..Inner;\n"
            "}\n"
            "use Base., Child fallback .Base;\n"
            "path one./*gap*/two;\n"
            "expr Child(Base.Inner, Base..Leaf);\n"),
        make_single_document_scenario(
            "stress/unclosed-comment-inside-call.stress",
            "fuzz-unclosed-comment-inside-call.stress", "stress-language",
            "module DeepComment\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "expr Ref(1, Ref(2, /* unterminated\n"
            "legacy (Ref + 3\n"),
        make_single_document_scenario(
            "stress/decl-recovery-cascade.stress",
            "fuzz-decl-recovery-cascade.stress", "stress-language",
            "module Cascade\n"
            "decl Base extends {\n"
            "  self Base;\n"
            "  many items: Base\n"
            "decl Child extends Base {\n"
            "  child: ;\n"
            "}\n"
            "use Base, Child fallback ;\n"
            "path Base..Child;\n"
            "expr Child(Base, 1 + );\n"),
        make_single_document_scenario(
            "stress/comment-cut-qualified-tail.stress",
            "fuzz-comment-cut-qualified-tail.stress", "stress-language",
            "module TailCut\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Child extends Base/* gap */.Inner {\n"
            "  child: Base/* cut */\n"
            "}\n"
            "use Base/* gap */, Child fallback Base/* eof\n"),
        make_single_document_scenario(
            "stress/deep-mixed-nesting.stress",
            "fuzz-deep-mixed-nesting.stress", "stress-language",
            "module DeepMixed\n"
            "export decl Root {\n"
            "  many children: Root;\n"
            "  self: Root;\n"
            "}\n"
            "decl Leaf extends Root {\n"
            "  child: Root;\n"
            "}\n"
            "use Root, Leaf fallback Root;\n"
            "export choose command;\n"
            "bag gamma beta alpha;\n"
            "peek alpha;\n"
            "guard token;\n"
            "path root.branch.leaf;\n"
            "doc \"nested \\\"quoted\\\" value\";\n"
            "setting unsafe = on;\n"
            "tuple(one, two, three);\n"
            "expr Leaf(Root(1 + 2, Leaf(3, 4)), (5 + 6) * (7 - 8));\n"
            "expr Root(Leaf(Root, 1 + 2), Leaf(3, 4));\n"
            "legacy Root + Leaf - Root * Leaf + Root;\n"),
        make_single_document_scenario(
            "stress/keyword-punctuation-recovery.stress",
            "fuzz-keyword-punctuation-recovery.stress", "stress-language",
            "module KeywordChaos\n"
            "export decl Root extends {\n"
            "  many children Root;\n"
            "  self: Root;;\n"
            "}\n"
            "decl Leaf extends Root {\n"
            "  child: Root\n"
            "}\n"
            "use Root Leaf fallback .Leaf\n"
            "choose command event;\n"
            "bag alpha beta;\n"
            "peek alpha gamma;\n"
            "guard guard=;\n"
            "path root..branch.;\n"
            "setting safe on;\n"
            "tuple(one two,, three);\n"
            "expr Leaf(Root, (1 + ) * );\n"
            "legacy Root + /* gap */ - Leaf;\n"),
        make_single_document_scenario(
            "stress/comment-string-delimiter-cascade.stress",
            "fuzz-comment-string-delimiter-cascade.stress", "stress-language",
            "module StringCommentMix\n"
            "decl Ref {\n"
            "  self: Ref;\n"
            "}\n"
            "doc \"open slash \\\\\";\n"
            "doc 'single value';\n"
            "tuple(one, /* comment */, two);\n"
            "expr Ref(1, Ref(2, \"text\"), (3 + 4));\n"
            "expr Ref(1 /* gap */, Ref(2, 3 + ));\n"
            "legacy Ref /* gap */ + /* hole */\n"
            "/* unterminated comment after dense prefix\n"),
        make_single_document_scenario(
            "stress/qualified-call-cascade.stress",
            "fuzz-qualified-call-cascade.stress", "stress-language",
            "module QualifiedCascade\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Inner extends Base {\n"
            "  many items: Base;\n"
            "  child: Base;\n"
            "}\n"
            "use Base, Inner fallback Base;\n"
            "path Base.Inner.Tail;\n"
            "expr Inner(Base.Inner, Inner(Base, (1 + 2), Inner(3, 4)));\n"
            "expr Inner(Base/*cut*/.Inner, (Base + 1), Inner(2, 3 + ));\n"
            "legacy Base.Inner + Base - Inner * Base;\n"),
        make_single_document_scenario(
            "stress/unordered-predicate-extends-chaos.stress",
            "fuzz-unordered-predicate-extends-chaos.stress", "stress-language",
            "module StructuralChaos\n"
            "decl Base {\n"
            "  self: Base;\n"
            "}\n"
            "decl Child extends Base {\n"
            "  many items: Base;\n"
            "  child: Child;\n"
            "}\n"
            "decl Broken extends Base {\n"
            "  child Child;\n"
            "}\n"
            "bag gamma alpha beta;\n"
            "bag alpha /* cut */ gamma;\n"
            "peek beta;\n"
            "peek gamma alpha;\n"
            "guard guardToken;\n"
            "guard =;\n"
            "use Base, Child fallback Broken;\n"
            "path Base.Child..Leaf;\n"
            "expr Child(Base, Child(1, 2 + ));\n"
            "legacy Child + Broken - Base * ;\n"),
    };
  }();
  return scenarios;
}

const std::vector<WorkspaceScenarioSpec> &adversarial_single_document_scenarios() {
  static const auto scenarios = [] {
    return std::vector<WorkspaceScenarioSpec>{
        make_single_document_scenario(
            "adversarial/coverage.adv", "fuzz-coverage.adv",
            "adversarial-language",
            "graph Demo\n"
            "export node Root when yes && no {\n"
            "  many children: Root = Root;\n"
            "  self: Root = Root<Root>(Root);\n"
            "}\n"
            "node Leaf extends Root when Root<Root>(Root) {\n"
            "  child: Root = [Root, Root<Root>(Root)];\n"
            "}\n"
            "alias Main = Root;\n"
            "link Root -> Leaf when Root<Root>(Root) || yes;\n"
            "mix warm hot cold dry;\n"
            "probe fast;\n"
            "pack[one, two, three];\n"
            "case nested when Root {\n"
            "  eval Root<Root>(Root, {key: Root, other: [1, yes]});\n"
            "}\n"
            "eval Root<Root>(Root, {key: Root, other: [1, yes]});\n"
            "legacy Root + Leaf * Root && Root;\n"),
        make_single_document_scenario(
            "adversarial/recovery-dense.adv", "fuzz-recovery-dense.adv",
            "adversarial-language",
            "graph Broken\n"
            "node Root extends when Root {\n"
            "  many children Root = ;\n"
            "  self: = Root<Root(;\n"
            "}\n"
            "alias Main = ;\n"
            "link Root -> when ;\n"
            "mix hot warm;\n"
            "probe fast deep;\n"
            "pack[one two,,];\n"
            "case nested when Root {\n"
            "  eval Root<Root>(Root, {key Root, other: [1, yes]});\n"
            "}\n"
            "eval Root<Root>(Root, {key: Root, other: [1, yes]};\n"
            "legacy Root + * Leaf;\n"),
        make_single_document_scenario(
            "adversarial/generic-prefix-chaos.adv",
            "fuzz-generic-prefix-chaos.adv", "adversarial-language",
            "graph Generic\n"
            "node Root {\n"
            "  self: Root = Root<Root, Root>(Root<Root>(Root));\n"
            "}\n"
            "node Leaf extends Root {\n"
            "  child: Root = Root<Root>(Root<Root, Root>(Root));\n"
            "}\n"
            "eval Root<Root, Leaf>(Root<Root>(Leaf), Root<Leaf>(Root));\n"
            "legacy Root(Root, Leaf) + Root * Leaf;\n"),
        make_single_document_scenario(
            "adversarial/map-list-delimiter-chaos.adv",
            "fuzz-map-list-delimiter-chaos.adv", "adversarial-language",
            "graph Collections\n"
            "node Root {\n"
            "  self: Root = {head: Root, tail: [Root, Root<Root>(Root)]};\n"
            "}\n"
            "pack[one, two, two, three];\n"
            "eval {head: Root, tail: [Root, {nested: Root}, Root<Root>(Root)]};\n"
            "eval [Root, {nested: [1, 2, 3]}, Root<Root>(Root)];\n"),
        make_single_document_scenario(
            "adversarial/nested-case-recursion.adv",
            "fuzz-nested-case-recursion.adv", "adversarial-language",
            "graph Cases\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "case outer when Root {\n"
            "  case inner when Root<Root>(Root) {\n"
            "    eval Root<Root>(Root, [Root, {k: Root}]);\n"
            "    legacy Root + Root * Root;\n"
            "  }\n"
            "}\n"),
        make_single_document_scenario(
            "adversarial/predicate-unordered-fragment.adv",
            "fuzz-predicate-unordered-fragment.adv", "adversarial-language",
            "graph Predicates\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "mix hot warm;\n"
            "mix cold dry hot;\n"
            "probe fast slow;\n"
            "probe =;\n"
            "eval Root && && Root;\n"),
        make_single_document_scenario(
            "adversarial/qualified-link-fragment.adv",
            "fuzz-qualified-link-fragment.adv", "adversarial-language",
            "graph Links\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "node Leaf extends Root {\n"
            "  child: Root = Root;\n"
            "}\n"
            "alias Main = Root::Leaf;\n"
            "link Root::Leaf -> Root:: when Root::Leaf<Root::Leaf>(Root);\n"
            "eval Root::Leaf<Root::Leaf>(Root::Leaf);\n"),
        make_single_document_scenario(
            "adversarial/unclosed-comment-call-tail.adv",
            "fuzz-unclosed-comment-call-tail.adv", "adversarial-language",
            "graph CommentTail\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "eval Root<Root>(Root, {key: Root, tail: [Root, /* unterminated\n"),
        make_single_document_scenario(
            "adversarial/eof-generic-tail.adv",
            "fuzz-eof-generic-tail.adv", "adversarial-language",
            "graph Tail\n"
            "node Root {\n"
            "  self: Root = Root<Root>(Root\n"),
        make_single_document_scenario(
            "adversarial/legacy-operator-chaos.adv",
            "fuzz-legacy-operator-chaos.adv", "adversarial-language",
            "graph Legacy\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "legacy Root + + Leaf;\n"
            "legacy Root && * Leaf;\n"
            "legacy (Root + Leaf;\n"),
        make_single_document_scenario(
            "adversarial/brace-angle-comment-cascade.adv",
            "fuzz-brace-angle-comment-cascade.adv", "adversarial-language",
            "graph Cascade\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "node Leaf extends Root when Root<Root>(Root) {\n"
            "  child: Root = {left: Root<Root>(Root), tail: [Root, "
            "Root<Leaf>(Root, {k: Root /* gap\n"),
        make_single_document_scenario(
            "adversarial/case-link-pack-overlap.adv",
            "fuzz-case-link-pack-overlap.adv", "adversarial-language",
            "graph Overlap\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "case outer when Root {\n"
            "  link Root -> Root when Root<Root>(Root;\n"
            "  pack[one, two three];\n"
            "  probe fast =;\n"
            "}\n"),
        make_single_document_scenario(
            "adversarial/qualified-generic-call-chaos.adv",
            "fuzz-qualified-generic-call-chaos.adv", "adversarial-language",
            "graph Qualified\n"
            "node Root {\n"
            "  self: Root = Root;\n"
            "}\n"
            "node Inner extends Root {\n"
            "  child: Root = Root;\n"
            "}\n"
            "eval Root::Inner<Root::Inner>(Root::Inner<Root>(Root::Inner), "
            "{left: [Root::Inner, Root::Inner<Root>(Root::Inner)], right: "
            "Root::Inner});\n"
            "link Root::Inner -> Root when Root::Inner<Root::Inner>(Root);\n"),
        make_single_document_scenario(
            "adversarial/multi-error-node-case.adv",
            "fuzz-multi-error-node-case.adv", "adversarial-language",
            "graph Multi\n"
            "export node Root extends Root when Root && {\n"
            "  many child Root = ;\n"
            "  leaf: Root = {tail: [Root, Root<Root>(Root)] ;\n"
            "}\n"
            "case outer when Root {\n"
            "  case inner when Root<Root>(Root {\n"
            "    eval Root<Root>(Root, {tail: [Root, Leaf]});\n"
            "  }\n"
            "}\n"
            "link Root -> Root when ;\n"),
    };
  }();
  return scenarios;
}

const std::vector<WorkspaceScenarioSpec> &adversarial_workspace_scenarios() {
  static const auto scenarios = [] {
    return std::vector<WorkspaceScenarioSpec>{
        make_workspace_scenario(
            "adversarial/workspace-relink-basic",
            {
                ScenarioDocumentSpec{
                    .uri = test::make_file_uri("fuzz-adversarial-provider.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Provider\n"
                        "export node Root {\n"
                        "  self: Root = Root;\n"
                        "}\n"
                        "node Leaf extends Root when Root<Root>(Root) {\n"
                        "  child: Root = Root;\n"
                        "}\n"
                        "alias Main = Leaf;\n",
                },
                ScenarioDocumentSpec{
                    .uri = test::make_file_uri("fuzz-adversarial-consumer.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Consumer\n"
                        "node Mirror extends Root when Root<Leaf>(Root) {\n"
                        "  branch: Leaf = Leaf;\n"
                        "}\n"
                        "link Root -> Leaf when Root<Leaf>(Root) || yes;\n"
                        "case nested when Leaf {\n"
                        "  eval Root<Leaf>(Leaf, {left: Root, right: [Leaf]});\n"
                        "}\n"
                        "legacy Root + Leaf * Root;\n",
                },
            }),
        make_workspace_scenario(
            "adversarial/workspace-relink-cascade",
            {
                ScenarioDocumentSpec{
                    .uri = test::make_file_uri("fuzz-adversarial-core.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Core\n"
                        "node Root {\n"
                        "  self: Root = Root;\n"
                        "}\n"
                        "node Inner extends Root {\n"
                        "  child: Root = Root;\n"
                        "}\n",
                },
                ScenarioDocumentSpec{
                    .uri =
                        test::make_file_uri("fuzz-adversarial-overlay.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Overlay\n"
                        "alias Main = Inner;\n"
                        "node Proxy extends Inner when Inner<Root>(Root) {\n"
                        "  edge: Root = {left: Root, right: [Inner, Root]};\n"
                        "}\n"
                        "pack[one, two, three];\n"
                        "probe fast;\n"
                        "eval Inner<Root>(Root, {tail: [Inner, Root<Inner>(Root)]});\n"
                        "link Inner -> Root when Inner<Root>(Root);\n",
                },
            }),
        make_workspace_scenario(
            "adversarial/workspace-relink-three-hop",
            {
                ScenarioDocumentSpec{
                    .uri = test::make_file_uri("fuzz-adversarial-base.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Base\n"
                        "export node Root {\n"
                        "  self: Root = Root;\n"
                        "}\n"
                        "node Leaf extends Root {\n"
                        "  child: Root = Root;\n"
                        "}\n",
                },
                ScenarioDocumentSpec{
                    .uri =
                        test::make_file_uri("fuzz-adversarial-middle.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Middle\n"
                        "alias Main = Leaf;\n"
                        "node Bridge extends Leaf when Leaf<Root>(Root) {\n"
                        "  edge: Root = {left: Root, tail: [Leaf, Root]};\n"
                        "}\n"
                        "link Bridge -> Root when Bridge<Leaf>(Leaf);\n",
                },
                ScenarioDocumentSpec{
                    .uri =
                        test::make_file_uri("fuzz-adversarial-consumer3.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph Consumer3\n"
                        "node Mirror extends Bridge when Bridge<Leaf>(Leaf) {\n"
                        "  branch: Leaf = Leaf<Root>(Root);\n"
                        "}\n"
                        "case nested when Bridge {\n"
                        "  eval Bridge<Leaf>(Leaf, {left: Root, right: [Bridge, Leaf]});\n"
                        "}\n"
                        "legacy Bridge + Leaf * Root;\n",
                },
            }),
        make_workspace_scenario(
            "adversarial/workspace-relink-fanout",
            {
                ScenarioDocumentSpec{
                    .uri = test::make_file_uri("fuzz-adversarial-fanout-base.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph FanoutBase\n"
                        "export node Root {\n"
                        "  self: Root = Root;\n"
                        "}\n"
                        "node Leaf extends Root {\n"
                        "  child: Root = Root;\n"
                        "}\n",
                },
                ScenarioDocumentSpec{
                    .uri =
                        test::make_file_uri("fuzz-adversarial-fanout-overlay.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph FanoutOverlay\n"
                        "alias Shared = Leaf;\n"
                        "node Bridge extends Shared when Shared<Root>(Root) {\n"
                        "  edge: Root = {left: Root, right: [Shared, Root]};\n"
                        "}\n"
                        "pack[one, two, three];\n",
                },
                ScenarioDocumentSpec{
                    .uri =
                        test::make_file_uri("fuzz-adversarial-fanout-rules.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph FanoutRules\n"
                        "link Bridge -> Shared when Bridge<Shared>(Shared) || yes;\n"
                        "case fanout when Shared {\n"
                        "  eval Shared<Root>(Root, {left: Bridge, right: [Shared, Root]});\n"
                        "}\n",
                },
                ScenarioDocumentSpec{
                    .uri =
                        test::make_file_uri("fuzz-adversarial-fanout-consumer.adv"),
                    .languageId = "adversarial-language",
                    .text =
                        "graph FanoutConsumer\n"
                        "node Mirror extends Bridge when Bridge<Shared>(Shared) {\n"
                        "  branch: Shared = Shared<Root>(Root);\n"
                        "}\n"
                        "eval Bridge<Shared>(Shared, {tail: [Mirror, Bridge, Shared]});\n"
                        "legacy Bridge + Shared * Root && Mirror;\n",
                },
            }),
    };
  }();
  return scenarios;
}

void expect_workspace_round_trip(const WorkspaceScenarioSpec &scenario,
                                 std::size_t targetIndex,
                                 std::string_view mutationProgram) {
  using Clock = std::chrono::steady_clock;
  ASSERT_LT(targetIndex, scenario.documents.size());
  SCOPED_TRACE(scenario.name);
  SCOPED_TRACE("target=" + scenario.documents[targetIndex].uri);
  SCOPED_TRACE("mutation_size=" + std::to_string(mutationProgram.size()));
  SCOPED_TRACE("mutation_hex=" + hex_summary(mutationProgram));

  auto mutatedText = scenario.documents[targetIndex].text;
  mutate_text(mutatedText, mutationProgram);

  try {
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] roundTrip scenario=%s stage=before-initialize "
                   "target=%zu mutationHex=%s\n",
                   scenario.name.c_str(), targetIndex,
                   hex_string(mutationProgram).c_str());
    }
    WorkspaceScenarioRunner runner(scenario);
    runner.initialize();
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] roundTrip scenario=%s stage=after-initialize\n",
                   scenario.name.c_str());
    }

    const auto baselineStart = Clock::now();
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] roundTrip scenario=%s stage=before-build-initial\n",
                   scenario.name.c_str());
    }
    const auto baselineSnapshots = runner.build_initial();
    const auto baselineElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - baselineStart);
    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] roundTrip scenario=%s stage=after-build-initial "
                   "ms=%lld\n",
                   scenario.name.c_str(),
                   static_cast<long long>(baselineElapsed.count()));
    }
    ASSERT_EQ(baselineSnapshots.size(), scenario.documents.size());
    trace_snapshot("baseline", baselineSnapshots[targetIndex]);

    const auto mutatedStart = Clock::now();
    const auto mutatedSnapshots = runner.update_document(targetIndex, mutatedText);
    const auto mutatedElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - mutatedStart);
    ASSERT_EQ(mutatedSnapshots.size(), scenario.documents.size());
    trace_snapshot("mutated", mutatedSnapshots[targetIndex]);

    const auto rebuiltStart = Clock::now();
    const auto rebuiltSnapshots =
        runner.update_document(targetIndex, mutatedSnapshots[targetIndex].text);
    const auto rebuiltElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - rebuiltStart);
    trace_snapshot("rebuilt", rebuiltSnapshots[targetIndex]);
    EXPECT_EQ(rebuiltSnapshots, mutatedSnapshots);

    const auto restoredStart = Clock::now();
    const auto restoredSnapshots =
        runner.update_document(targetIndex, scenario.documents[targetIndex].text);
    const auto restoredElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - restoredStart);
    trace_snapshot("restored", restoredSnapshots[targetIndex]);
    EXPECT_EQ(restoredSnapshots, baselineSnapshots);

    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] scenario=%s baselineMs=%lld mutatedMs=%lld "
                   "rebuiltMs=%lld restoredMs=%lld mutationHex=%s\n",
                   scenario.name.c_str(),
                   static_cast<long long>(baselineElapsed.count()),
                   static_cast<long long>(mutatedElapsed.count()),
                   static_cast<long long>(rebuiltElapsed.count()),
                   static_cast<long long>(restoredElapsed.count()),
                   hex_string(mutationProgram).c_str());
    }
  } catch (const std::exception &exception) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "target_uri=" << scenario.documents[targetIndex].uri << "\n"
           << "mutation_hex=" << hex_string(mutationProgram) << "\n"
           << "mutated_text=" << quoted_text(mutatedText) << "\n"
           << "what=" << exception.what();
  } catch (...) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "target_uri=" << scenario.documents[targetIndex].uri << "\n"
           << "mutation_hex=" << hex_string(mutationProgram) << "\n"
           << "mutated_text=" << quoted_text(mutatedText) << "\n"
           << "what=non-std exception";
  }
}

void expect_cached_workspace_round_trip(const WorkspaceScenarioSpec &scenario,
                                        std::size_t targetIndex,
                                        std::string_view mutationProgram) {
  using Clock = std::chrono::steady_clock;
  ASSERT_LT(targetIndex, scenario.documents.size());
  SCOPED_TRACE(scenario.name);
  SCOPED_TRACE("target=" + scenario.documents[targetIndex].uri);
  SCOPED_TRACE("mutation_size=" + std::to_string(mutationProgram.size()));
  SCOPED_TRACE("mutation_hex=" + hex_summary(mutationProgram));

  auto mutatedText = scenario.documents[targetIndex].text;
  mutate_text(mutatedText, mutationProgram);

  try {
    auto &state = ensure_cached_workspace_round_trip_state(scenario);
    ASSERT_NE(state.runner, nullptr);
    auto &runner = *state.runner;
    const auto &baselineSnapshots = state.baselineSnapshots;
    ASSERT_EQ(baselineSnapshots.size(), scenario.documents.size());
    trace_snapshot("baseline", baselineSnapshots[targetIndex]);

    const auto mutatedStart = Clock::now();
    const auto mutatedSnapshots = runner.update_document(targetIndex, mutatedText);
    const auto mutatedElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - mutatedStart);
    ASSERT_EQ(mutatedSnapshots.size(), scenario.documents.size());
    trace_snapshot("mutated", mutatedSnapshots[targetIndex]);

    const auto rebuiltStart = Clock::now();
    const auto rebuiltSnapshots =
        runner.update_document(targetIndex, mutatedSnapshots[targetIndex].text);
    const auto rebuiltElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - rebuiltStart);
    trace_snapshot("rebuilt", rebuiltSnapshots[targetIndex]);
    EXPECT_EQ(rebuiltSnapshots, mutatedSnapshots);

    const auto restoredStart = Clock::now();
    const auto restoredSnapshots =
        runner.update_document(targetIndex, scenario.documents[targetIndex].text);
    const auto restoredElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - restoredStart);
    trace_snapshot("restored", restoredSnapshots[targetIndex]);
    EXPECT_EQ(restoredSnapshots, baselineSnapshots);

    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] cachedScenario=%s mutatedMs=%lld "
                   "rebuiltMs=%lld restoredMs=%lld mutationHex=%s\n",
                   scenario.name.c_str(),
                   static_cast<long long>(mutatedElapsed.count()),
                   static_cast<long long>(rebuiltElapsed.count()),
                   static_cast<long long>(restoredElapsed.count()),
                   hex_string(mutationProgram).c_str());
    }
  } catch (const std::exception &exception) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "target_index=" << targetIndex << "\n"
           << "target_uri=" << scenario.documents[targetIndex].uri << "\n"
           << "mutation_hex=" << hex_summary(mutationProgram) << "\n"
           << "mutated_text=" << quoted_text(mutatedText) << "\n"
           << "what=" << exception.what();
  } catch (...) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "target_index=" << targetIndex << "\n"
           << "target_uri=" << scenario.documents[targetIndex].uri << "\n"
           << "mutation_hex=" << hex_summary(mutationProgram) << "\n"
           << "mutated_text=" << quoted_text(mutatedText) << "\n"
           << "what=non-std exception";
  }
}

void expect_stress_document_build(std::string_view text) {
  using Clock = std::chrono::steady_clock;
  auto scenario = make_single_document_scenario(
      "stress/generated.stress", "fuzz-generated.stress", "stress-language",
      std::string(text));
  WorkspaceScenarioRunner runner(scenario);
  runner.initialize();

  try {
    const auto baselineStart = Clock::now();
    const auto baselineSnapshots = runner.build_initial();
    const auto baselineElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - baselineStart);
    ASSERT_EQ(baselineSnapshots.size(), 1u);
    trace_snapshot("baseline", baselineSnapshots.front());

    const auto rebuiltStart = Clock::now();
    const auto rebuiltSnapshots = runner.update_document(0u, std::string(text));
    const auto rebuiltElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - rebuiltStart);
    trace_snapshot("rebuilt", rebuiltSnapshots.front());
    EXPECT_EQ(rebuiltSnapshots, baselineSnapshots);

    if (fuzz_trace_enabled()) {
      std::fprintf(stderr,
                   "[fuzz-trace] scenario=%s baselineMs=%lld rebuiltMs=%lld\n",
                   scenario.name.c_str(),
                   static_cast<long long>(baselineElapsed.count()),
                   static_cast<long long>(rebuiltElapsed.count()));
    }
  } catch (const std::exception &exception) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "document_text=" << quoted_text(text) << "\n"
           << "what=" << exception.what();
  } catch (...) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "document_text=" << quoted_text(text) << "\n"
           << "what=non-std exception";
  }
}

void expect_adversarial_document_build(std::string_view text) {
  auto scenario = make_single_document_scenario(
      "adversarial/generated.adv", "fuzz-generated.adv",
      "adversarial-language", std::string(text));
  WorkspaceScenarioRunner runner(scenario);
  runner.initialize();

  try {
    const auto baselineSnapshots = runner.build_initial();
    ASSERT_EQ(baselineSnapshots.size(), 1u);

    const auto rebuiltSnapshots = runner.update_document(0u, std::string(text));
    EXPECT_EQ(rebuiltSnapshots, baselineSnapshots);
  } catch (const std::exception &exception) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "document_text=" << quoted_text(text) << "\n"
           << "what=" << exception.what();
  } catch (...) {
    FAIL() << "scenario=" << scenario.name << "\n"
           << "document_text=" << quoted_text(text) << "\n"
           << "what=non-std exception";
  }
}

} // namespace pegium::fuzz
