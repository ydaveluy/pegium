#pragma once

#include <algorithm>
#include <array>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/Group.hpp>
#include <pegium/parser/NotPredicate.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <ranges>

namespace pegium::parser {

struct CharacterRange final : grammar::CharacterRange {
  using type = std::string_view;
  constexpr ~CharacterRange() override = default;
  constexpr explicit CharacterRange(std::string_view s)
      : lookup{createCharacterRange(s)} {}

  constexpr CharacterRange(CharacterRange &&) = default;
  constexpr CharacterRange(const CharacterRange &) = default;
  constexpr CharacterRange &operator=(CharacterRange &&) = default;
  constexpr CharacterRange &operator=(const CharacterRange &) = default;

  constexpr bool parse_rule(ParseState &s) const {
    const char *const begin = s.cursor();
    if (begin == s.end ||
        !lookup[static_cast<unsigned char>(*begin)]) {
      return false;
    }
    // word boundary should be checked with `+ !w`
    /*if (sv.end() > i.offset && isWord(*(i.offset - 1)) && isWord(*i.offset)) {
      return MatchResult::failure(i.offset);
    }*/
    s.leaf(begin + 1, this);
    s.skipHiddenNodes();
    return true;
  }
  bool recover(RecoverState &recoverState) const {
    const char *const begin = recoverState.cursor();
    if (begin != recoverState.end &&
        lookup[static_cast<unsigned char>(*begin)]) {
      recoverState.leaf(begin + 1, this);
      recoverState.skipHiddenNodes();
      return true;
    }

    if (recoverState.isStrictNoEditMode()) {
      return false;
    }

    const auto mark = recoverState.mark();
    while (recoverState.deleteOneCodepoint()) {
      const char *const scan = recoverState.cursor();
      if (scan != recoverState.end &&
          lookup[static_cast<unsigned char>(*scan)]) {
        recoverState.leaf(scan + 1, this);
        recoverState.skipHiddenNodes();
        return true;
      }
    }

    recoverState.rewind(mark);
    return false;
  }
  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {

    return (begin != end && lookup[static_cast<unsigned char>(*begin)])
               ? MatchResult::success(begin + 1)
               : MatchResult::failure(begin);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
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
