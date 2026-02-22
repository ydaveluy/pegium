#pragma once

#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/TextUtils.hpp>

namespace pegium::parser {

struct AnyCharacter final : grammar::AnyCharacter {
  using type = std::string_view;

  constexpr bool parse_rule(ParseState &s) const {
    const char *begin = s.cursor();
    const char *end = s.end;
    if (begin == end) [[unlikely]] {
      return false;
    }

    if (const auto len = utf8_codepoint_length(*begin);
        static_cast<std::size_t>(end - begin) >= len) [[likely]] {
      s.leaf(begin + len, this);
      s.skipHiddenNodes();
      return true;
    }
    return false;
  }
  bool recover(RecoverState &recoverState) const {
    const char *const begin = recoverState.cursor();
    const char *const end = recoverState.end;
    if (begin != end) [[likely]] {
      if (const auto len = utf8_codepoint_length(*begin);
          static_cast<std::size_t>(end - begin) >= len) [[likely]] {
        recoverState.leaf(begin + len, this);
        recoverState.skipHiddenNodes();
        return true;
      }
    }

    if (recoverState.isStrictNoEditMode()) {
      return false;
    }

    const auto mark = recoverState.mark();
    if (recoverState.insertHidden(this)) {
      recoverState.skipHiddenNodes();
      return true;
    }

    recoverState.rewind(mark);
    while (recoverState.deleteOneCodepoint()) {
      const char *const scan = recoverState.cursor();
      if (scan == end) {
        continue;
      }
      if (const auto len = utf8_codepoint_length(*scan);
          static_cast<std::size_t>(end - scan) >= len) [[likely]] {
        recoverState.leaf(scan + len, this);
        recoverState.skipHiddenNodes();
        return true;
      }
    }

    recoverState.rewind(mark);
    return false;
  }
  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    if (begin == end) [[unlikely]] {
      return MatchResult::failure(begin);
    }

    if (const auto len = utf8_codepoint_length(*begin);
        static_cast<std::size_t>(end - begin) >= len) [[likely]] {
      return MatchResult::success(begin + len);
    }
    return MatchResult::failure(begin);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }

};
} // namespace pegium::parser
