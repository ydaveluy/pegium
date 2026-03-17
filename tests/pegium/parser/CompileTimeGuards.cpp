#include <pegium/parser/PegiumParser.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

using namespace pegium::parser;

namespace {

enum class GuardEnum : std::uint8_t { Value = 1 };

struct RuleNode : pegium::AstNode {
  string text;
  bool enabled = false;
};

struct ListNode : pegium::AstNode {
  vector<string> values;
};

struct ParentNode : pegium::AstNode {
  pointer<RuleNode> child;
};

struct TypedRawValueExpr final : pegium::grammar::AbstractElement {
  using type = std::string;
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<typed-raw>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return text.c_str();
  }
  std::string getRawValue(const pegium::CstNodeView &) const { return "v"; }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &) const { return true; }
};

struct GenericRawValueExpr final : pegium::grammar::AbstractElement {
  using type = std::string;
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<generic-raw>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return text.c_str();
  }
  pegium::grammar::RuleValue getRawValue(const pegium::CstNodeView &) const {
    return pegium::grammar::RuleValue{std::string_view{"v"}};
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &) const { return true; }
};

struct VariantRawValueExpr final : pegium::grammar::AbstractElement {
  using type = std::string;
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<variant-raw>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return text.c_str();
  }
  std::variant<std::string, bool>
  getRawValue(const pegium::CstNodeView &) const {
    return std::variant<std::string, bool>{std::string{"v"}};
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &) const { return true; }
};

struct LegacyRecoverSignatureExpr final : pegium::grammar::AbstractElement {
  using type = std::string;
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<legacy-recover>"; }
  bool rule(ParseContext &) const { return true; }
  bool recover(ParseContext &) const { return true; }
  bool expect(ExpectContext &) const { return true; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return text.c_str();
  }
};

struct MissingStrictProbeSafeExpr final : pegium::grammar::AbstractElement {
  using type = std::string;
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<missing-probe-safe>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return text.c_str();
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &) const { return true; }
};

using NonNullableExpr = decltype("a"_kw);
using NullableExpr = decltype(option("a"_kw));
using AssignmentExpr = decltype(assign<&RuleNode::text>("a"_kw));
using CreateExpr = decltype(create<RuleNode>());
using NestExpr = decltype(nest<ParentNode, &ParentNode::child>());
using DataTypeExpr = decltype(DataTypeRule<std::string>{"D", "a"_kw});
using ParserExpr = decltype(ParserRule<RuleNode>{"P", "a"_kw});
using GroupWithAssignmentExpr =
    decltype("a"_kw + assign<&RuleNode::text>("b"_kw));
using ChoiceWithAssignmentExpr =
    decltype("a"_kw | assign<&RuleNode::text>("b"_kw));
using UnorderedWithAssignmentExpr =
    decltype("a"_kw & assign<&RuleNode::text>("b"_kw));
using StrictChoiceExpr = decltype("a"_kw | "b"_kw);
using StrictAndExpr = decltype(&"a"_kw);
using StrictNotExpr = decltype(!"a"_kw);
using StrictInfixOperatorExpr = decltype(LeftAssociation("+"_kw | "-"_kw));

template <typename Expr>
concept CanCreateTerminalRule = requires {
  TerminalRule<>{std::string_view{"T"}, std::declval<Expr>()};
};

template <typename Expr>
concept CanCreateDataTypeRule = requires {
  DataTypeRule<std::string>{std::string_view{"R"}, std::declval<Expr>()};
};

template <typename Expr>
concept CanCreateParserRule = requires {
  ParserRule<RuleNode>{std::string_view{"R"}, std::declval<Expr>()};
};

template <typename Expr>
concept CanHide = requires { hidden(std::declval<Expr>()); };

template <typename Expr>
concept CanIgnore = requires { ignored(std::declval<Expr>()); };

template <typename Expr>
concept CanCreateAssign = requires {
  assign<&RuleNode::text>(std::declval<Expr>());
};

template <typename Expr>
concept HasSpecificAssignmentRawValue =
    requires(const std::remove_cvref_t<Expr> &expr,
             const pegium::CstNodeView &node) {
      expr.getRawValue(node);
    } &&
    (!std::same_as<std::remove_cvref_t<decltype(
          std::declval<const std::remove_cvref_t<Expr> &>().getRawValue(
              std::declval<const pegium::CstNodeView &>()))>,
                   pegium::grammar::RuleValue>) &&
    (!pegium::parser::detail::IsStdVariant<std::remove_cvref_t<decltype(
          std::declval<const std::remove_cvref_t<Expr> &>().getRawValue(
              std::declval<const pegium::CstNodeView &>()))>>::value);

template <typename Expr>
concept CanCreateAppend = requires {
  append<&ListNode::values>(std::declval<Expr>());
};

template <typename Expr>
concept CanCreateEnableIf = requires {
  enable_if<&RuleNode::enabled>(std::declval<Expr>());
};

template <typename Expr>
concept CanCreateMany = requires {
  many(std::declval<Expr>());
};

template <typename Expr>
concept CanCreateRepeat = requires {
  repeat<2>(std::declval<Expr>());
};

template <typename Expr>
concept CanCreateTerminalRuleWithConverterOption = requires {
  TerminalRule<int>{
      std::string_view{"T"}, std::declval<Expr>(),
      opt::with_converter(
          [](std::string_view) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_value<int>(1);
          })};
};

template <typename Expr>
concept CanCreateDataTypeRuleWithSkipperOption = requires {
  DataTypeRule<std::string>{std::string_view{"R"}, std::declval<Expr>(),
                            opt::with_skipper(SkipperBuilder().build())};
};

template <typename Expr>
concept CanCreateDataTypeRuleWithConverterOption = requires {
  DataTypeRule<int>{
      std::string_view{"R"}, std::declval<Expr>(),
      opt::with_converter(
          [](const pegium::CstNodeView &) noexcept
              -> opt::ConversionResult<int> {
            return opt::conversion_value<int>(1);
          })};
};

template <typename Expr>
concept CanCreateTerminalRuleWithLegacyConverterOption = requires {
  TerminalRule<int>{
      std::string_view{"T"}, std::declval<Expr>(),
      opt::with_converter([](std::string_view) { return 1; })};
};

template <typename Expr>
concept CanCreateDataTypeRuleWithLegacyConverterOption = requires {
  DataTypeRule<int>{
      std::string_view{"R"}, std::declval<Expr>(),
      opt::with_converter([](const pegium::CstNodeView &) { return 1; })};
};

template <typename Expr>
concept CanCreateParserRuleWithSkipperOption = requires {
  ParserRule<RuleNode>{std::string_view{"R"}, std::declval<Expr>(),
                       opt::with_skipper(SkipperBuilder().build())};
};

template <typename Expr>
concept CanProbeStrictly = requires(const std::remove_cvref_t<Expr> &expr,
                                    ParseContext &ctx) {
  { probe(expr, ctx) } -> std::same_as<bool>;
};

static_assert(StrictParseModeContext<ParseContext>);
static_assert(StrictParseModeContext<TrackedParseContext>);
static_assert(!StrictParseModeContext<RecoveryContext>);
static_assert(RecoveryParseModeContext<RecoveryContext>);
static_assert(ExpectParseModeContext<ExpectContext>);

template <typename Expr>
concept HasTerminalApi = requires(const std::remove_cvref_t<Expr> &expr,
                                  const char *begin) {
  { expr.terminal(begin) } noexcept -> std::same_as<const char *>;
};

static_assert(!NonNullableExpr::nullable);
static_assert(NullableExpr::nullable);
static_assert(TerminalCapableExpression<NonNullableExpr>);
static_assert(TerminalAtom<NonNullableExpr>);
static_assert(!TerminalCapableExpression<AssignmentExpr>);
static_assert(!TerminalCapableExpression<CreateExpr>);
static_assert(!TerminalCapableExpression<NestExpr>);
static_assert(!TerminalCapableExpression<DataTypeExpr>);
static_assert(!TerminalCapableExpression<ParserExpr>);
static_assert(!TerminalCapableExpression<GroupWithAssignmentExpr>);
static_assert(!TerminalCapableExpression<ChoiceWithAssignmentExpr>);
static_assert(!TerminalCapableExpression<UnorderedWithAssignmentExpr>);
static_assert(CanProbeStrictly<NonNullableExpr>);
static_assert(CanProbeStrictly<NullableExpr>);
static_assert(CanProbeStrictly<AssignmentExpr>);
static_assert(CanProbeStrictly<CreateExpr>);
static_assert(CanProbeStrictly<NestExpr>);
static_assert(CanProbeStrictly<DataTypeExpr>);
static_assert(CanProbeStrictly<ParserExpr>);
static_assert(CanProbeStrictly<GroupWithAssignmentExpr>);
static_assert(CanProbeStrictly<ChoiceWithAssignmentExpr>);
static_assert(CanProbeStrictly<UnorderedWithAssignmentExpr>);
static_assert(CanProbeStrictly<StrictChoiceExpr>);
static_assert(CanProbeStrictly<StrictAndExpr>);
static_assert(CanProbeStrictly<StrictNotExpr>);
static_assert(CanProbeStrictly<StrictInfixOperatorExpr>);

static_assert(CanCreateTerminalRule<NonNullableExpr>);
static_assert(CanHide<NonNullableExpr>);
static_assert(CanIgnore<NonNullableExpr>);
static_assert(CanCreateDataTypeRule<NonNullableExpr>);
static_assert(CanCreateParserRule<NonNullableExpr>);
static_assert(CanCreateAssign<NonNullableExpr>);
static_assert(CanCreateAssign<TypedRawValueExpr>);
static_assert(HasSpecificAssignmentRawValue<TypedRawValueExpr>);
static_assert(!HasSpecificAssignmentRawValue<VariantRawValueExpr>);
static_assert(CanCreateAppend<NonNullableExpr>);
static_assert(CanCreateEnableIf<NonNullableExpr>);
static_assert(CanCreateMany<NonNullableExpr>);
static_assert(CanCreateRepeat<NonNullableExpr>);
static_assert(detail::SupportedRuleValueType<GuardEnum>);
static_assert(CanCreateTerminalRuleWithConverterOption<NonNullableExpr>);
static_assert(CanCreateDataTypeRuleWithSkipperOption<NonNullableExpr>);
static_assert(CanCreateDataTypeRuleWithConverterOption<NonNullableExpr>);
static_assert(CanCreateParserRuleWithSkipperOption<NonNullableExpr>);
static_assert(requires {
  ("a"_kw + "b"_kw).skip(ignored(some(s)));
});
static_assert(requires {
  ("a"_kw | "b"_kw).skip(ignored(some(s)));
});
static_assert(requires {
  ("a"_kw & "b"_kw).skip(ignored(some(s)));
});
static_assert(requires {
  some("a"_kw).skip(ignored(some(s)));
});

static_assert(!CanCreateTerminalRule<NullableExpr>);
static_assert(!CanCreateTerminalRule<AssignmentExpr>);
static_assert(!CanCreateTerminalRule<CreateExpr>);
static_assert(!CanCreateTerminalRule<NestExpr>);
static_assert(!CanCreateTerminalRule<DataTypeExpr>);
static_assert(!CanCreateTerminalRule<ParserExpr>);
static_assert(!CanCreateTerminalRule<GroupWithAssignmentExpr>);
static_assert(!CanCreateTerminalRule<ChoiceWithAssignmentExpr>);
static_assert(!CanCreateTerminalRule<UnorderedWithAssignmentExpr>);
static_assert(!CanHide<AssignmentExpr>);
static_assert(!CanIgnore<AssignmentExpr>);
static_assert(!HasTerminalApi<AssignmentExpr>);
static_assert(!HasTerminalApi<CreateExpr>);
static_assert(!HasTerminalApi<NestExpr>);
static_assert(!HasTerminalApi<DataTypeExpr>);
static_assert(!HasTerminalApi<ParserExpr>);
static_assert(!CanCreateDataTypeRule<NullableExpr>);
static_assert(!CanCreateParserRule<NullableExpr>);
static_assert(!CanCreateAssign<NullableExpr>);
static_assert(!HasSpecificAssignmentRawValue<GenericRawValueExpr>);
static_assert(!CanCreateAppend<NullableExpr>);
static_assert(!CanCreateEnableIf<NullableExpr>);
static_assert(!CanCreateMany<NullableExpr>);
static_assert(!CanCreateRepeat<NullableExpr>);
static_assert(!CanCreateTerminalRuleWithConverterOption<NullableExpr>);
static_assert(!Expression<LegacyRecoverSignatureExpr>);
static_assert(Expression<MissingStrictProbeSafeExpr>);
static_assert(!CanCreateDataTypeRuleWithSkipperOption<NullableExpr>);
static_assert(!CanCreateDataTypeRuleWithConverterOption<NullableExpr>);
static_assert(!CanCreateParserRuleWithSkipperOption<NullableExpr>);

} // namespace
