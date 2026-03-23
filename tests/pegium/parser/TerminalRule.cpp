#include <cstdint>
#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <ranges>
#include <variant>

using namespace pegium::parser;

namespace {

enum class TerminalMode : std::uint16_t { Single = 42 };

template <typename T> struct TerminalValueNode : pegium::AstNode {
  T value{};
};

template <typename T> struct TerminalPairNode : pegium::AstNode {
  T first{};
  T second{};
};

template <typename T>
ParseResult parseTerminalRule(const TerminalRule<T> &rule, std::string_view text,
                              const Skipper &skipper,
                              const ParseOptions &options = {}) {
  ParserRule<TerminalValueNode<T>> root{
      "Root", assign<&TerminalValueNode<T>::value>(rule)};
  return pegium::test::parse_rule_result(root, text, skipper, options);
}

} // namespace

TEST(TerminalRuleTest, ParseRequiresFullConsumption) {
  TerminalRule<std::string_view> terminal{"T", "hello"_kw};

  {
    auto result = parseTerminalRule(terminal, "hello", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
        result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "hello");
  }

  {
    auto result =
        parseTerminalRule(terminal, "helloX", SkipperBuilder().build());
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
    auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
        result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "hello");
  }
}

TEST(TerminalRuleTest, IntegralConversionUsesFromChars) {
  TerminalRule<int> number{"Number", some(d)};

  {
    auto result = parseTerminalRule(number, "12345", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, 12345);
  }

  {
    auto result =
        parseTerminalRule(number, "12345x", SkipperBuilder().build());
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, 12345);
  }
}

TEST(TerminalRuleTest, ParseRuleLeavesCursorAtTokenEnd) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};

  auto builderHarness = pegium::test::makeCstBuilderHarness("abc   ");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().ignore(ws).build();

  ParseContext ctx{builder, skipper};
  auto result = parse(terminal, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 3);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), "abc");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(TerminalRuleTest, BoolConversionMapsTrueAndFalse) {
  TerminalRule<bool> flag{"Flag", "true"_kw | "false"_kw};

  auto yes = parseTerminalRule(flag, "true", SkipperBuilder().build());
  ASSERT_TRUE(yes.value);
  auto *yesTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(yes.value);
  ASSERT_TRUE(yesTyped != nullptr);
  EXPECT_TRUE(yesTyped->value);

  auto no = parseTerminalRule(flag, "false", SkipperBuilder().build());
  ASSERT_TRUE(no.value);
  auto *noTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(no.value);
  ASSERT_TRUE(noTyped != nullptr);
  EXPECT_FALSE(noTyped->value);
}

TEST(TerminalRuleTest, FloatingPointConversionAndFailurePaths) {
  TerminalRule<double> number{"Number", some(dot)};

  {
    auto result = parseTerminalRule(number, "12.5", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<double>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_DOUBLE_EQ(typed->value, 12.5);
  }
}

TEST(TerminalRuleTest, CharRuleCanUseCustomConverter) {
  TerminalRule<char> ch{"Char", "x"_kw};

  ch.setValueConverter([](std::string_view sv) noexcept {
    return opt::conversion_value<char>(sv.empty() ? '\0' : sv.front());
  });

  auto result = parseTerminalRule(ch, "x", SkipperBuilder().build());
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<char>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 'x');
}

TEST(TerminalRuleTest, ConstructorOptionCanSetValueConverter) {
  TerminalRule<int> number{"Number", "42"_kw,
                           opt::with_converter([](std::string_view sv) noexcept {
                             return opt::conversion_value<int>(
                                 static_cast<int>(sv.size()) + 1);
                           })};

  auto result = parseTerminalRule(number, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 3);
}

TEST(TerminalRuleTest, ConverterCanSetValue) {
  TerminalRule<int> number{
      "Number", "42"_kw,
      opt::with_converter(
          [](std::string_view sv) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_value<int>(static_cast<int>(sv.size()) + 2);
          })};

  auto result = parseTerminalRule(number, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.parseDiagnostics.empty());

  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 4);
}

TEST(TerminalRuleTest,
     ConverterFailureProducesConversionDiagnosticAndFallbackValue) {
  TerminalRule<int> number{
      "Number", "42"_kw,
      opt::with_converter(
          [](std::string_view) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_error<int>("bad number");
          })};

  auto result = parseTerminalRule(number, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics.front().message, "bad number");

  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 0);
}

TEST(TerminalRuleTest, MultipleConverterFailuresProduceOrderedDiagnostics) {
  TerminalRule<int> number{"Number", some(d),
                           opt::with_converter([](std::string_view) noexcept
                                                   -> opt::ConversionResult<int> {
                             return opt::conversion_error<int>("bad number");
                           })};
  ParserRule<TerminalPairNode<int>> root{
      "Root", assign<&TerminalPairNode<int>::first>(number) + " "_kw +
                  assign<&TerminalPairNode<int>::second>(number)};

  const auto result =
      pegium::test::parse_rule_result(root, "1 2", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 2u);
  EXPECT_EQ(result.parseDiagnostics[0].kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics[1].kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics[0].offset, 0u);
  EXPECT_EQ(result.parseDiagnostics[1].offset, 2u);
  EXPECT_EQ(result.parseDiagnostics[0].message, "bad number");
  EXPECT_EQ(result.parseDiagnostics[1].message, "bad number");

  auto *typed =
      pegium::ast_ptr_cast<TerminalPairNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->first, 0);
  EXPECT_EQ(typed->second, 0);
}

TEST(TerminalRuleTest, RecoveredConverterFailureStillProducesDiagnostic) {
  TerminalRule<int> number{"Number", "1"_kw,
                           opt::with_converter([](std::string_view sv) noexcept
                                                   -> opt::ConversionResult<int> {
                             if (sv != "1") {
                               return opt::conversion_error<int>(
                                   "bad recovered number");
                             }
                             return opt::conversion_value<int>(1);
                           })};
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  builder.leaf(0, static_cast<pegium::TextOffset>(builder.getText().size()),
               std::addressof(number), false, true);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  const auto node = *it;
  ASSERT_TRUE(node.isRecovered());

  std::vector<ParseDiagnostic> diagnostics;
  const ValueBuildContext context{.diagnostics = &diagnostics};
  EXPECT_EQ(number.getRawValue(node, context), 0);
  EXPECT_TRUE(std::ranges::any_of(
      diagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::ConversionError &&
               diagnostic.message == "bad recovered number";
      }));
}

TEST(TerminalRuleTest, ParseRuleFailureLeavesCursorAndTreeUntouched) {
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};
  auto builderHarness = pegium::test::makeCstBuilderHarness("abX");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(terminal, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(TerminalRuleTest, RecoveryInsertDoesNotMaterializeHiddenCstNodes) {
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};
  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  ASSERT_TRUE(parse(terminal, ctx));
  const auto diagnostics =
      RecoveryContext::materializeRecoveryEdits(ctx.snapshotRecoveryEdits());
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().kind, ParseDiagnosticKind::Inserted);
  EXPECT_EQ(ctx.cursorOffset(), 0u);

  const auto *root = builder.getRootCstNode();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->begin(), root->end());
}

TEST(TerminalRuleTest, CanOverrideRule) {
  TerminalRule<bool> rule{"RuleWithOverrides", "a"_kw};
  auto skipper = SkipperBuilder().build();
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper).fullMatch);

  // first override
  rule = rule.super() | "b"_kw;
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper).fullMatch);

  // second override
  rule = rule.super() | "c"_kw;
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper).fullMatch);

  // third override
  rule = "d"_kw;
  EXPECT_FALSE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "d", skipper).fullMatch);
}

TEST(TerminalRuleTest, EnumGetValueUsesUnderlyingVariantType) {
  TerminalRule<TerminalMode> mode{"Mode", "m"_kw};
  mode.setValueConverter([](std::string_view) noexcept {
    return opt::conversion_value<TerminalMode>(TerminalMode::Single);
  });

  auto parsed = parseTerminalRule(mode, "m", SkipperBuilder().build());
  ASSERT_TRUE(parsed.value);
  auto *typed =
      pegium::ast_ptr_cast<TerminalValueNode<TerminalMode>>(parsed.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, TerminalMode::Single);

  auto skipper = SkipperBuilder().build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("m");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, skipper};
  ASSERT_TRUE(parse(mode, ctx));
  auto root = builder.getRootCstNode();
  auto node = root->begin();
  ASSERT_NE(node, root->end());

  auto value = mode.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::uint16_t>(value));
  EXPECT_EQ(std::get<std::uint16_t>(value), static_cast<std::uint16_t>(42));
}
