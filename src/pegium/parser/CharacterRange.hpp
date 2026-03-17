#pragma once

#include <algorithm>
#include <array>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/CompletionSupport.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/Group.hpp>
#include <pegium/parser/NotPredicate.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/TerminalRecoverySupport.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <ranges>
#include <string>

namespace pegium::parser {

template <auto range>
struct CharacterRange final : grammar::CharacterRange,
                              CompletionTerminalMatcher {
  static constexpr std::array<bool, 256> lookup =
      createCharacterRange({range.data(), range.size()});
  static_assert(!lookup[0],
                "CharacterRange cannot include '\\0'.");
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;
  using type = std::string_view;
  std::string_view getRawValue(const CstNodeView &node) const noexcept {
    return getValue(node);
  }

  constexpr const char *terminal(const char *begin) const noexcept {

    return matches(begin) ? begin + 1 : nullptr;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return terminal(text.c_str());
  }

  [[nodiscard]] const char *
  matchForCompletion(const char *begin) const noexcept override {
    return terminal(begin);
  }

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

  void print(std::ostream &os) const override {
    os << '[';
    int rangeStart = -1;
    for (int codepoint = 0; codepoint < 256; ++codepoint) {
      if (lookup[codepoint]) {
        if (rangeStart == -1)
          rangeStart = codepoint;
      } else {
        if (rangeStart != -1) {
          if (codepoint - rangeStart == 1)
            os << escape_char(static_cast<char>(rangeStart));
          else if (codepoint - rangeStart == 2)
            os << escape_char(static_cast<char>(rangeStart))
               << escape_char(static_cast<char>(rangeStart + 1));
          else
            os << escape_char(static_cast<char>(rangeStart)) << '-'
               << escape_char(static_cast<char>(codepoint - 1));
          rangeStart = -1;
        }
      }
    }
    os << ']';
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;

  bool probe_impl(ParseContext &ctx) const noexcept {
    return matches(ctx.cursor());
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      const char *const cursorStart = ctx.cursor();
      if (matches(cursorStart)) {
        ctx.leaf(cursorStart + 1, this);
        return true;
      }
      return false;
    } else if constexpr (RecoveryParseModeContext<Context>) {
      const char *const cursorStart = ctx.cursor();
      if (matches(cursorStart)) {
        ctx.leaf(cursorStart + 1, this);
        return true;
      }
      if (!ctx.canEdit()) {
        return false;
      }
      return detail::apply_delete_scan_terminal_candidate(
          ctx,
          [&](const char *scanCursor) noexcept -> const char * {
            return matches(scanCursor) ? scanCursor + 1 : nullptr;
          },
          [&](const char *matchedEnd) { ctx.leaf(matchedEnd, this); });
    } else {
      if (ctx.reachedAnchor()) {
        return false;
      }
      if (matches(ctx.cursor())) {
        ctx.leaf(ctx.cursor() + 1, this);
        return true;
      }
      if (!ctx.canEdit()) {
        return false;
      }
      return detail::apply_delete_scan_terminal_candidate(
          ctx,
          [&](const char *scanCursor) noexcept -> const char * {
            const auto *matchedEnd =
                matches(scanCursor) ? scanCursor + 1 : nullptr;
            return detail::can_apply_recovery_match(ctx, matchedEnd)
                       ? matchedEnd
                       : nullptr;
          },
          [&](const char *matchedEnd) { ctx.leaf(matchedEnd, this); });
    }
  }

  static constexpr bool matches(const char *cursor) noexcept {
    return lookup[static_cast<unsigned char>(*cursor)];
  }
};

namespace detail {

template <auto range>
struct IsTerminalAtom<CharacterRange<range>> : std::true_type {};

} // namespace detail

} // namespace pegium::parser
