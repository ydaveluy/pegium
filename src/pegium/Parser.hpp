#pragma once

#include <pegium/grammar/Action.hpp>
#include <pegium/grammar/AndPredicate.hpp>
#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/grammar/Context.hpp>
#include <pegium/grammar/DataTypeRule.hpp>
#include <pegium/grammar/Group.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/grammar/OrderedChoice.hpp>
#include <pegium/grammar/ParserRule.hpp>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/grammar/TerminalRule.hpp>
#include <pegium/grammar/UnorderedGroup.hpp>
#include <pegium/IParser.hpp>
#include <pegium/syntax-tree.hpp>
#include <type_traits>

namespace pegium::grammar {

/// any character equivalent to regex `.`
static constexpr AnyCharacter dot{};
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
template <typename T, typename U>
  requires IsGrammarElement<T> && IsGrammarElement<U>
constexpr auto operator<=>(T &&from, U &&to) {
  return std::forward<T>(from) + many(!std::forward<U>(to) + dot) +
         std::forward<U>(to);
}
} // namespace pegium::grammar

namespace pegium {
class Parser : public IParser {
public:
  GenericParseResult parse(const std::string &input) const override {
    if (!entryRule)
      throw std::logic_error("The entry rule is not defined");
    return entryRule->parseGeneric(input, createContext());
  }
  std::unique_ptr<pegium::grammar::IContext> createContext() const override {
    return ContextBuilder().build();
  }

private:
  template <typename T, typename = void> struct RuleHelper {
    static_assert(sizeof(T) == 0, "Unsupported type for Rule");
  };

  template <typename T>
  struct RuleHelper<
      T,
      std::enable_if_t<std::same_as<T, bool> || std::same_as<T, std::string> ||
                       std::is_integral_v<T> || std::is_enum_v<T>>> {
    using type = pegium::grammar::DataTypeRule<T>;
  };

  template <typename T>
  struct RuleHelper<T, std::enable_if_t<std::derived_from<T, AstNode>>> {
    using type = pegium::grammar::ParserRule<T>;
  };
  template <typename T>
  struct RuleHelper<std::shared_ptr<T>,
                    std::enable_if_t<std::derived_from<T, AstNode>>> {
    using type = pegium::grammar::ParserRule<std::shared_ptr<T>>;
  };
  const grammar::IRule *entryRule = nullptr;

protected:
  void setEntryRule(const grammar::IRule *entryRule) {
    this->entryRule = entryRule;
  }
  void setEntryRule(const grammar::IRule &entryRule) {
    this->entryRule = &entryRule;
  }
  /*template <typename T = std::string>
  using TerminalRule = pegium::grammar::TerminalRule<T>;
  template <typename T = std::string>
  using DataTypeRule = pegium::grammar::DataTypeRule<T>;
  template <typename T>
   // requires std::derived_from<T, AstNode>
  using ParserRule = pegium::grammar::ParserRule<T>;*/

  template <typename T = std::string_view>
  using Terminal = pegium::grammar::TerminalRule<T>;

  // Alias principal
  template <typename T = std::string> using Rule = typename RuleHelper<T>::type;

  using ContextBuilder = pegium::grammar::ContextBuilder<>;
};

} // namespace pegium