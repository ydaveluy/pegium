#pragma once

#include <algorithm>
#include <array>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/parser/AbstractElement.hpp>
#include <ranges>

namespace pegium::parser {

struct CharacterRange final : grammar::CharacterRange {
  using type = std::string;
 // constexpr ~CharacterRange() override = default;
  constexpr CharacterRange(std::string_view s) : lookup{createCharacterRange(s)} {}

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {
    auto i = parse_terminal(sv);
    if (!i) {
      return i;
    }
    // word boundary should be checked with `+ !w`
    /*if (sv.end() > i.offset && isword(*(i.offset - 1)) && isword(*i.offset)) {
      return MatchResult::failure(i.offset);
    }*/
    auto &node = parent.content.emplace_back();
    node.text = {sv.data(), i.offset};
    node.grammarSource = this;

    return c.skipHiddenNodes({i.offset, sv.end()}, parent);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {

    return (!sv.empty() && lookup[static_cast<unsigned char>(sv[0])])
               ? MatchResult::success(sv.begin() + 1)
               : MatchResult::failure(sv.begin());
  }

  /// Create an insensitive Characters Ranges
  /// @return the insensitive Characters Ranges
  constexpr CharacterRange &i() noexcept {
    for (char c = 'a'; c <= 'z'; ++c) {
      auto lower = static_cast<unsigned char>(c);
      auto upper = static_cast<unsigned char>(c - 'a' + 'A');

      lookup[lower] |= lookup[upper];
      lookup[upper] |= lookup[lower];
    }
    return *this;
  }

  void print(std::ostream &os) const override {
    auto escape_char = [](unsigned char c) -> std::string {
      switch (c) {
      case '\n':
        return R"(\n)";
      case '\r':
        return R"(\r)";
      case '\t':
        return R"(\t)";
      case '\v':
        return R"(\v)";
      case '\f':
        return R"(\f)";
      case '\b':
        return R"(\b)";
      case '\a':
        return R"(\a)";
      case '\\':
        return R"(\\)";
      case '\'':
        return R"(\')";
      case '\"':
        return R"(\")";
      default:
        if (std::isprint(c)) {
          return std::string{static_cast<char>(c)};
        } else {
          char buf[5];
          std::snprintf(buf, sizeof(buf), "\\x%02X", c);
          return buf;
        }
      }
    };
    os << '[';
    int start = -1;
    for (int i = 0; i < 256; ++i) {
      if (lookup[i]) {
        if (start == -1)
          start = i;
      } else {
        if (start != -1) {
          if (i - start == 1)
            os << escape_char(start);
          else if (i - start == 2)
            os << escape_char(start) << escape_char(start + 1);
          else
            os << escape_char(start) << '-' << escape_char(i - 1);
          start = -1;
        }
      }
    }
    os << ']';
  }
private:
  std::array<bool, 256> lookup{};
};

constexpr CharacterRange operator""_cr(char const *s, std::size_t len) {
  return CharacterRange{std::string_view{s, len}};
}

} // namespace pegium::parser