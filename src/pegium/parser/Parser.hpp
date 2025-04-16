#pragma once

#include <pegium/parser/Action.hpp>
#include <pegium/parser/AndPredicate.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/CharacterRange.hpp>
#include <pegium/parser/Context.hpp>
#include <pegium/parser/CrossReference.hpp>
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
#include <pegium/syntax-tree.hpp>
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

/// The end of file token
static constexpr auto eof = !dot;
/// The end of line token
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
/// `to`. e.g `"#*"_kw >> "*#"_kw` to match a multiline comment
/// @param from the starting element
/// @param to the ending element
/// @return the until element
template <ParserExpression T, ParserExpression U>
constexpr auto operator<=>(T &&from, U &&to) {
  return std::forward<T>(from) + many(!std::forward<U>(to) + dot) +
         std::forward<U>(to);
}
} // namespace pegium::parser

namespace pegium::parser {
class Parser : public IParser {
public:
  /*GenericParseResult parse(const std::string &input) const override {
    if (!entryRule)
      throw std::logic_error("The entry rule is not defined");
    return entryRule->parseGeneric(input, createContext());
  }*/
  std::unique_ptr<IContext> createContext() const override {
    return ContextBuilder().build();
  }

private:
  template <typename T, typename = void> struct RuleHelper {
    static_assert(sizeof(T) == 0, "Unsupported type for Rule");
  };

  template <typename T>
  struct RuleHelper<T, std::enable_if_t<!std::derived_from<T, AstNode>>> {
    using type = DataTypeRule<T>;
  };

  template <typename T>
  struct RuleHelper<T, std::enable_if_t<std::derived_from<T, AstNode>>> {
    using type = ParserRule<T>;
  };

  const grammar::Rule *entryRule = nullptr;

protected:
  /*void setEntryRule(const grammar::IRule *entryRule) {
    this->entryRule = entryRule;
  }*/
  void setEntryRule(const grammar::Rule &entryRule) {
    this->entryRule = &entryRule;
  }

  template <typename T = std::string_view> using Terminal = TerminalRule<T>;
  template <typename T = std::string> using Rule = typename RuleHelper<T>::type;

  // using ContextBuilder = ContextBuilder<>;
};

} // namespace pegium::parser