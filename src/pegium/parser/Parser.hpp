#pragma once

#include <pegium/parser/Action.hpp>
#include <pegium/parser/AndPredicate.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/CharacterRange.hpp>
#include <pegium/parser/Context.hpp>
#include <pegium/parser/DataTypeRule.hpp>
#include <pegium/parser/Group.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Literal.hpp>
#include <pegium/parser/NotPredicate.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <pegium/parser/ParserRule.hpp>
#include <pegium/parser/Repetition.hpp>
#include <pegium/parser/TerminalRule.hpp>
#include <pegium/parser/UnorderedGroup.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <type_traits>

namespace pegium::parser {

/// any character equivalent to regex `.`
static constexpr AnyCharacter dot;

/// Create a CharacterRange
template <char_array_builder builder> consteval auto operator""_cr() {
  static_assert(!builder.value.empty(), "A CharacterRange cannot be empty.");

  if constexpr (builder.value[0] == '^') {
    return !CharacterRange{std::string_view{builder.value.data() + 1,
                                            builder.value.size() - 1}} +
           dot;
  } else {
    return CharacterRange{
        std::string_view{builder.value.data(), builder.value.size()}};
  }
}

/// Create a Keyword
template <char_array_builder builder> consteval auto operator""_kw() {
  static_assert(!builder.value.empty(), "A keyword cannot be empty.");
  return Literal<builder.value>{};
}

/// The end of file
static constexpr auto eof = !dot;
/// The end of line
static constexpr auto eol = "\n"_kw | "\r\n"_kw | "\r"_kw;
/// a space character equivalent to regex `\s`
static constexpr auto s = " \t\r\n\f\v"_cr;
/// a non space character equivalent to regex `\S`
static constexpr auto S = !s + dot;
/// a word character equivalent to regex `\w`
static constexpr auto w = "a-zA-Z0-9_"_cr;
/// a non word character equivalent to regex `\W`
static constexpr auto W = !w + dot;
/// a digit character equivalent to regex `\d`
static constexpr auto d = "0-9"_cr;
/// a non-digit character equivalent to regex `\D`
static constexpr auto D = !d + dot;

/// An until operation that starts from element `from` and ends to element
/// `to`. e.g `"from"_kw <=> "to"_kw`.
/// This operation is non-greedy, it will stop as soon as it finds the `to`
/// element.
/// @param from the starting element
/// @param to the ending element
/// @return the until element
template <ParseExpression T, ParseExpression U>
constexpr auto operator<=>(T &&from, U &&to) {
  return std::forward<T>(from) + many(!std::forward<U>(to) + dot) +
         std::forward<U>(to);
}
} // namespace pegium::parser

namespace pegium::parser {
class Parser : public IParser {
public:
  auto createContext() const {
    return SkipperBuilder().build();
  }

private:
  template <typename T>
  struct RuleHelper {
    static_assert(sizeof(T) == 0, "Unsupported type for Rule");
  };

  template <typename T>
    requires (!std::derived_from<T, AstNode>)
  struct RuleHelper<T> {
    using type = DataTypeRule<T>;
  };

  template <typename T>
    requires std::derived_from<T, AstNode>
  struct RuleHelper<T> {
    using type = ParserRule<T>;
  };

  const grammar::AbstractRule *entryRule = nullptr;

protected:
  void setEntryRule(const grammar::AbstractRule &entryRule) {
    this->entryRule = &entryRule;
  }

  template <typename T = std::string_view> using Terminal = TerminalRule<T>;
  template <typename T = std::string> using Rule = typename RuleHelper<T>::type;

};

} // namespace pegium::parser
