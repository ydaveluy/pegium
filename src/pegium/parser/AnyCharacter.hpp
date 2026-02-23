#pragma once

#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/TextUtils.hpp>

namespace pegium::parser {

struct AnyCharacter final : grammar::AnyCharacter {
  static constexpr bool nullable = false;

  using type = std::string_view;

  bool rule(ParseContext &ctx) const {
    const char *const begin = ctx.cursor();
    const char *const end = ctx.end;
    if (begin != end) [[likely]] {
      if (const auto len = utf8_codepoint_length(*begin);
          static_cast<std::size_t>(end - begin) >= len) [[likely]] {
        ctx.leaf(begin + len, this);
        ctx.skipHiddenNodes();
        return true;
      }
    }

    if (ctx.isStrictNoEditMode()) {
      return false;
    }

    const auto mark = ctx.mark();
    if (ctx.insertHidden(this)) {
      ctx.skipHiddenNodes();
      return true;
    }
    // TODO move mark at this place because insertHidden does not change the state when returning false, so we can avoid the mark and rewind in this case
    ctx.rewind(mark);
    while (ctx.deleteOneCodepoint()) {
      const char *const scan = ctx.cursor();
      if (scan == end) {
        continue;
      }
      if (const auto len = utf8_codepoint_length(*scan);
          static_cast<std::size_t>(end - scan) >= len) [[likely]] {
        ctx.leaf(scan + len, this);
        ctx.skipHiddenNodes();
        return true;
      }
    }

    ctx.rewind(mark);
    return false;
  }
  constexpr MatchResult terminal(const char *begin,
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
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }

};
} // namespace pegium::parser
