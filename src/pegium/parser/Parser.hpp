#pragma once

#include <pegium/parser/Action.hpp>
#include <pegium/parser/AndPredicate.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/CharacterRange.hpp>
#include <pegium/parser/Context.hpp>
#include <pegium/parser/DataTypeRule.hpp>
#include <pegium/parser/Group.hpp>
#include <pegium/parser/Literal.hpp>
#include <pegium/parser/NotPredicate.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <pegium/parser/ParserRule.hpp>
#include <pegium/parser/Repetition.hpp>
#include <pegium/parser/TerminalRule.hpp>
// #include <pegium/parser/UnorderedGroup.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/syntax-tree.hpp>
#include <type_traits>

namespace pegium::parser {

/// any character equivalent to regex `.`
static constexpr AnyCharacter dot;
/// The end of file token
static constexpr auto eof = !dot;
/// The end of line token
static constexpr auto eol = "\r\n"_kw | "\n"_kw | "\r"_kw;
/// a space character equivalent to regex `\s`
static constexpr auto s = " \t\r\n\f\v"_cr;
/// a non space character equivalent to regex `\S`
static constexpr auto S = !s;
/// a word character equivalent to regex `\w`
static constexpr auto w = "a-zA-Z0-9_"_cr;
/// a non word character equivalent to regex `\W`
static constexpr auto W = !w;
/// a digit character equivalent to regex `\d`
static constexpr auto d = "0-9"_cr;
/// a non-digit character equivalent to regex `\D`
static constexpr auto D = !d;

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
} // namespace pegium::grammar

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

  template <typename T = std::string_view>
  using Terminal = TerminalRule<T>;
  template <typename T = std::string> using Rule = typename RuleHelper<T>::type;

  //using ContextBuilder = ContextBuilder<>;
};

} // namespace pegium::parser