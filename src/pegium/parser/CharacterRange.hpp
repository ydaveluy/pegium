#pragma once

#include <algorithm>
#include <array>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/Group.hpp>
#include <pegium/parser/NotPredicate.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <ranges>

namespace pegium::parser {

struct CharacterRange final : grammar::CharacterRange {
  static constexpr bool nullable = false;

  using type = std::string_view;
  constexpr ~CharacterRange() override = default;
  constexpr explicit CharacterRange(std::string_view s)
      : lookup{createCharacterRange(s)} {}

  constexpr CharacterRange(CharacterRange &&) = default;
  constexpr CharacterRange(const CharacterRange &) = default;
  constexpr CharacterRange &operator=(CharacterRange &&) = default;
  constexpr CharacterRange &operator=(const CharacterRange &) = default;

  bool rule(ParseContext &ctx) const {
    const char *const begin = ctx.cursor();
    if (begin != ctx.end &&
        lookup[static_cast<unsigned char>(*begin)]) {
      ctx.leaf(begin + 1, this);
      ctx.skipHiddenNodes();
      return true;
    }

    if (ctx.isStrictNoEditMode()) {
      return false;
    }

    const auto mark = ctx.mark();
    while (ctx.deleteOneCodepoint()) {
      const char *const scan = ctx.cursor();
      if (scan != ctx.end &&
          lookup[static_cast<unsigned char>(*scan)]) {
        ctx.leaf(scan + 1, this);
        ctx.skipHiddenNodes();
        return true;
      }
    }

    ctx.rewind(mark);
    return false;
  }
  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {

    return (begin != end && lookup[static_cast<unsigned char>(*begin)])
               ? MatchResult::success(begin + 1)
               : MatchResult::failure(begin);
  }
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }

  /// Create an insensitive Characters Ranges
  /// @return the insensitive Characters Ranges
  constexpr CharacterRange i() const noexcept {
    auto insensitive = *this;
    insensitive.make_insensitive();
    return insensitive;
  }

  void print(std::ostream &os) const override {
    os << '[';
    int start = -1;
    for (int i = 0; i < 256; ++i) {
      if (lookup[i]) {
        if (start == -1)
          start = i;
      } else {
        if (start != -1) {
          if (i - start == 1)
            os << escape_char(static_cast<char>(start));
          else if (i - start == 2)
            os << escape_char(static_cast<char>(start))
               << escape_char(static_cast<char>(start + 1));
          else
            os << escape_char(static_cast<char>(start)) << '-'
               << escape_char(static_cast<char>(i - 1));
          start = -1;
        }
      }
    }
    os << ']';
  }

private:
  constexpr void make_insensitive() noexcept {
    for (char c = 'a'; c <= 'z'; ++c) {
      auto lower = static_cast<unsigned char>(c);
      auto upper = static_cast<unsigned char>(c - 'a' + 'A');

      lookup[lower] |= lookup[upper];
      lookup[upper] |= lookup[lower];
    }
  }
  std::array<bool, 256> lookup{};
};

} // namespace pegium::parser
