#pragma once
#include <algorithm>
#include <array>
#include <pegium/grammar/Literal.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <ranges>

namespace pegium::parser {

template <auto literal, bool case_sensitive = true>
struct Literal final : grammar::Literal {
  static constexpr bool nullable = literal.empty();
  using type = std::string_view;
  using grammar::Literal::getValue;
  std::string_view getValue() const noexcept override {
    return {literal.begin(), literal.size()};
  }
  std::string_view getValue(const CstNodeView &node) const noexcept override {
    if (!node.isRecovered()) [[likely]] {
      return node.getText();
    }
    return getValue();
  }
  bool isCaseSensitive() const noexcept override { return case_sensitive; }

  bool rule(ParseContext &ctx) const {
    const char *const end = ctx.end;
    const char *boundaryOffset = nullptr;

    // Fast path: mirror parse_rule, and bail out immediately on exact match.
    const char *const begin = ctx.cursor();
    if (literal.size() <= static_cast<std::size_t>(end - begin)) {
      for (std::size_t i = 0; i < literal.size(); ++i) {
        if constexpr (case_sensitive) {
          if (begin[i] != literal[i]) {
            goto slow_path;
          }
        } else {
          if (tolower(begin[i]) != literal[i]) {
            goto slow_path;
          }
        }
      }
      const char *const offset = begin + literal.size();

      if constexpr (isWord(literal.back())) {
        if (offset != end && isWord(*offset)) {
          boundaryOffset = offset;
          goto slow_path;
        }
      }

      PEGIUM_RECOVERY_TRACE("[literal rule] direct match '", getValue(),
                            "' at ", ctx.cursorOffset());
      ctx.leaf(offset, this);
      ctx.skipHiddenNodes();
      return true;
    }

slow_path:
    if (ctx.isStrictNoEditMode()) {
      return false;
    }

    const auto mark = ctx.mark();
    if (boundaryOffset != nullptr) {
      PEGIUM_RECOVERY_TRACE("[literal rule] boundary violation '", getValue(),
                            "' at ", ctx.cursorOffset());
      const auto boundaryMark = ctx.mark();
      const char *const boundaryEnd =
          advanceOneCodepointLossy(boundaryOffset, ctx.end);
      ctx.leaf(boundaryOffset, this);
      if (ctx.deleteOneCodepoint()) {
        PEGIUM_RECOVERY_TRACE("[literal rule] boundary delete success '",
                              getValue(), "'");
        return true;
      }
      ctx.rewind(boundaryMark);
      if (boundaryEnd > ctx.cursor() &&
          ctx.replaceLeaf(boundaryEnd, this)) {
        PEGIUM_RECOVERY_TRACE("[literal rule] boundary replace success '",
                              getValue(), "'");
        ctx.skipHiddenNodes();
        return true;
      }
      ctx.rewind(mark);
    }
    if constexpr (recover_insertable) {
      if (ctx.insertHidden(this)) {
        PEGIUM_RECOVERY_TRACE("[literal rule] inserted '", getValue(),
                              "' at ", ctx.cursorOffset());
        ctx.skipHiddenNodes();
        return true;
      }
      ctx.rewind(mark);
    }
    if (ctx.insertHiddenForced(this)) {
      PEGIUM_RECOVERY_TRACE("[literal rule] forced inserted '", getValue(),
                            "' at ", ctx.cursorOffset());
      ctx.skipHiddenNodes();
      return true;
    }
    ctx.rewind(mark);

    if constexpr (recover_typo_replaceable) {
      const char *const typoEnd =
          typo_replace_end(ctx.cursor(), ctx.end);
      if (typoEnd != nullptr) {
        if (typoEnd != ctx.end && isWord(*typoEnd)) {
          // keep searching through delete paths
        } else if (ctx.replaceLeaf(typoEnd, this)) {
          PEGIUM_RECOVERY_TRACE("[literal rule] typo replace success '",
                                getValue(), "'");
          ctx.skipHiddenNodes();
          return true;
        }
      }
    }

    while (ctx.deleteOneCodepoint()) {
      const char *const scanBegin = ctx.cursor();
      if (literal.size() > static_cast<std::size_t>(end - scanBegin)) {
        continue;
      }

      for (std::size_t i = 0; i < literal.size(); ++i) {
        if constexpr (case_sensitive) {
          if (scanBegin[i] != literal[i]) {
            goto next_delete_scan;
          }
        } else {
          if (tolower(scanBegin[i]) != literal[i]) {
            goto next_delete_scan;
          }
        }
      }

      {
        const char *const scanEnd = scanBegin + literal.size();
        if constexpr (isWord(literal.back())) {
          if (scanEnd != end && isWord(*scanEnd)) {
            goto next_delete_scan;
          }
        }

        PEGIUM_RECOVERY_TRACE("[literal rule] delete-scan match '", getValue(),
                              "' at ",
                              ctx.cursorOffset());
        ctx.leaf(scanEnd, this);
        ctx.skipHiddenNodes();
        return true;
      }
    next_delete_scan:
      continue;
    }

    PEGIUM_RECOVERY_TRACE("[literal rule] fail '", getValue(), "' at ",
                          ctx.cursorOffset());
    ctx.rewind(mark);
    return false;
  }
  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {

    if (literal.size() > static_cast<std::size_t>(end - begin)) [[unlikely]] {
      return MatchResult::failure(begin);
    }

    for (std::size_t i = 0; i < literal.size(); ++i) {
      if constexpr (case_sensitive) {
        if (begin[i] != literal[i]) {
          return MatchResult::failure(begin + i);
        }
      } else {
        if (tolower(begin[i]) != literal[i]) {
          return MatchResult::failure(begin + i);
        }
      }
    }
    return MatchResult::success(begin + literal.size());
  }

  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }

  /// Create an insensitive Literal
  /// @return the insensitive Literal
  constexpr auto i() const noexcept {
    return Literal<toLower(), isCaseSensitive(literal)>{};
  }
  void print(std::ostream &os) const override {
    os << "'";
    for (auto c : literal)
      os << escape_char(c);
    os << "'";
    if (!case_sensitive)
      os << "i";
  }

private:
  static constexpr bool recover_insertable =
      literal.size() > 0 && !isWord(literal.back());
  static constexpr bool recover_typo_replaceable =
      literal.size() > 1 &&
      std::ranges::all_of(literal, [](char c) { return isWord(c); });

  static constexpr bool can_replace_with_typo_same_length(
      const char *begin, const char *end) noexcept {
    if (literal.empty()) {
      return false;
    }

    if (literal.size() > static_cast<std::size_t>(end - begin)) {
      return false;
    }

    std::size_t mismatchCount = 0;
    std::size_t firstMismatch = 0;
    std::size_t secondMismatch = 0;
    for (std::size_t idx = 0; idx < literal.size(); ++idx) {
      if constexpr (case_sensitive) {
        if (literal[idx] == begin[idx]) {
          continue;
        }
      } else {
        if (tolower(begin[idx]) == literal[idx]) {
          continue;
        }
      }
      if (mismatchCount == 0) {
        firstMismatch = idx;
      } else if (mismatchCount == 1) {
        secondMismatch = idx;
      }
      ++mismatchCount;
      if (mismatchCount > 2) {
        return false;
      }
    }

    if (mismatchCount == 1) {
      return true;
    }

    if (mismatchCount == 2 && secondMismatch == firstMismatch + 1) {
      if constexpr (case_sensitive) {
        return literal[firstMismatch] == begin[secondMismatch] &&
               literal[secondMismatch] == begin[firstMismatch];
      } else {
        return tolower(begin[secondMismatch]) == literal[firstMismatch] &&
               tolower(begin[firstMismatch]) == literal[secondMismatch];
      }
    }
    return false;
  }

  static constexpr bool can_replace_with_one_missing_char(
      const char *begin, const char *end) noexcept {
    if (literal.size() < 2) {
      return false;
    }
    const auto expectedInputLen = literal.size() - 1;
    if (static_cast<std::size_t>(end - begin) < expectedInputLen) {
      return false;
    }

    std::size_t litIdx = 0;
    std::size_t inputIdx = 0;
    bool skippedLiteralChar = false;
    while (litIdx < literal.size() && inputIdx < expectedInputLen) {
      if constexpr (case_sensitive) {
        if (literal[litIdx] == begin[inputIdx]) {
          ++litIdx;
          ++inputIdx;
          continue;
        }
      } else {
        if (tolower(begin[inputIdx]) == literal[litIdx]) {
          ++litIdx;
          ++inputIdx;
          continue;
        }
      }
      if (skippedLiteralChar) {
        return false;
      }
      skippedLiteralChar = true;
      ++litIdx;
    }

    if (!skippedLiteralChar) {
      // Missing the final character.
      return litIdx + 1 == literal.size() && inputIdx == expectedInputLen;
    }
    return inputIdx == expectedInputLen &&
           (litIdx == literal.size() || litIdx + 1 == literal.size());
  }

  static constexpr bool can_replace_with_one_extra_char(
      const char *begin, const char *end) noexcept {
    const auto expectedInputLen = literal.size() + 1;
    if (static_cast<std::size_t>(end - begin) < expectedInputLen) {
      return false;
    }

    std::size_t litIdx = 0;
    std::size_t inputIdx = 0;
    bool skippedInputChar = false;
    while (litIdx < literal.size() && inputIdx < expectedInputLen) {
      if constexpr (case_sensitive) {
        if (literal[litIdx] == begin[inputIdx]) {
          ++litIdx;
          ++inputIdx;
          continue;
        }
      } else {
        if (tolower(begin[inputIdx]) == literal[litIdx]) {
          ++litIdx;
          ++inputIdx;
          continue;
        }
      }
      if (skippedInputChar) {
        return false;
      }
      skippedInputChar = true;
      ++inputIdx;
    }

    if (!skippedInputChar) {
      // Extra final character.
      return litIdx == literal.size() && inputIdx + 1 == expectedInputLen;
    }
    return litIdx == literal.size() &&
           (inputIdx == expectedInputLen || inputIdx + 1 == expectedInputLen);
  }

  static constexpr const char *typo_replace_end(const char *begin,
                                                const char *end) noexcept {
    if (can_replace_with_typo_same_length(begin, end)) {
      return begin + literal.size();
    }
    if (can_replace_with_one_missing_char(begin, end)) {
      return begin + literal.size() - 1;
    }
    if (can_replace_with_one_extra_char(begin, end)) {
      return begin + literal.size() + 1;
    }
    return nullptr;
  }

  static constexpr auto toLower() {
    decltype(literal) newLiteral;
    std::ranges::transform(literal, newLiteral.begin(),
                           [](char c) { return tolower(c); });
    return newLiteral;
  }

  static constexpr bool isCaseSensitive(auto lit) {
    return std::ranges::none_of(lit, [](char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });
  }
};

template <typename T> struct IsLiteralImpl : std::false_type {};
template <auto literal, bool case_sensitive>
struct IsLiteralImpl<Literal<literal, case_sensitive>> : std::true_type {};

template <typename T>
concept IsLiteral = IsLiteralImpl<T>::value;

} // namespace pegium::parser
