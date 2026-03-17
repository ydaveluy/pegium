#pragma once

#include <pegium/parser/AndPredicate.hpp>
#include <pegium/parser/AnyCharacter.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/CharacterRange.hpp>
#include <pegium/parser/Create.hpp>
#include <pegium/parser/DataTypeRule.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/Group.hpp>
#include <pegium/parser/Parser.hpp>
#include <pegium/parser/InfixRule.hpp>
#include <pegium/parser/Literal.hpp>
#include <pegium/parser/Nest.hpp>
#include <pegium/parser/NotPredicate.hpp>
#include <pegium/parser/OrderedChoice.hpp>
#include <pegium/parser/ParserRule.hpp>
#include <pegium/parser/Repetition.hpp>
#include <pegium/parser/SkipperBuilder.hpp>
#include <pegium/parser/TerminalRule.hpp>
#include <pegium/parser/UnorderedGroup.hpp>
#include <pegium/services/DefaultCoreService.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/workspace/Document.hpp>
#include <type_traits>

namespace pegium::parser {

/// any character equivalent to regex `.`
static constexpr AnyCharacter dot;

/// Create a CharacterRange
template <char_array_builder builder> consteval auto operator""_cr() {
  static_assert(!builder.value.empty(), "A CharacterRange cannot be empty.");

  if constexpr (builder.value[0] == '^') {
    constexpr auto without_caret = array_substr<1>(builder.value);
    return !CharacterRange<without_caret>{} + dot;
  } else {
    return CharacterRange<builder.value>{};
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
template <Expression StartExpr, Expression EndExpr>
constexpr auto operator<=>(StartExpr &&startExpr, EndExpr &&endExpr) {
  return std::forward<StartExpr>(startExpr) +
         many(!std::forward<EndExpr>(endExpr) + dot) +
         std::forward<EndExpr>(endExpr);
}
} // namespace pegium::parser

namespace pegium::parser {
struct ParseContext;
struct RecoveryContext;
}

namespace pegium::services {
struct CoreServices;
}

namespace pegium::parser {

class PegiumParser : public Parser, protected services::DefaultCoreService {
public:
  PegiumParser() noexcept;
  explicit PegiumParser(const services::CoreServices &services) noexcept
      : services::DefaultCoreService(services) {}

  void parse(workspace::Document &,
             const utils::CancellationToken & = {}) const override;
  [[nodiscard]] ExpectResult expect(
      std::string_view text, TextOffset offset,
      const utils::CancellationToken &cancelToken = {}) const override;

private:
  template <typename ValueType> struct RuleTypeSelector {
    static_assert(sizeof(ValueType) == 0, "Unsupported type for Rule");
  };

  template <typename ValueType>
    requires(!std::derived_from<ValueType, AstNode>)
  struct RuleTypeSelector<ValueType> {
    using type = DataTypeRule<ValueType>;
  };

  template <typename ValueType>
    requires std::derived_from<ValueType, AstNode>
  struct RuleTypeSelector<ValueType> {
    using type = ParserRule<ValueType>;
  };

protected:
  virtual const grammar::ParserRule &getEntryRule() const noexcept = 0;
  virtual const Skipper &getSkipper() const noexcept { return NoOpSkipper(); }
  virtual ParseOptions getParseOptions() const noexcept { return {}; }

  template <typename ValueType = std::string_view>
  using Terminal = TerminalRule<ValueType>;
  template <typename ValueType = std::string>
  using Rule = typename RuleTypeSelector<ValueType>::type;
  template <typename T, auto Left, auto Op, auto Right>
    requires std::derived_from<T, AstNode>
  using Infix = InfixRule<T, Left, Op, Right>;
};

} // namespace pegium::parser
