#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>
#include <variant>

using namespace pegium::parser;

namespace {

enum class TerminalMode : std::uint16_t { Single = 42 };

template <typename T> struct TerminalValueNode : pegium::AstNode {
  T value{};
};

template <typename T>
auto parseTerminalRule(const TerminalRule<T> &rule, std::string_view text,
                       const Skipper &skipper,
                       const ParseOptions &options = {}) {
  ParserRule<TerminalValueNode<T>> root{
      "Root", assign<&TerminalValueNode<T>::value>(rule)};
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  root.parse(*document, skipper, {}, options);
  return std::move(document);
}

} // namespace

TEST(TerminalRuleTest, ParseRequiresFullConsumption) {
  TerminalRule<std::string_view> terminal{"T", "hello"_kw};

  {
    auto document =
        parseTerminalRule(terminal, "hello", SkipperBuilder().build());
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
        result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "hello");
  }

  {
    auto document =
        parseTerminalRule(terminal, "helloX", SkipperBuilder().build());
    auto &result = document->parseResult;
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
    auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
        result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "hello");
  }
}

TEST(TerminalRuleTest, IntegralConversionUsesFromChars) {
  TerminalRule<int> number{"Number", some(d)};

  {
    auto document =
        parseTerminalRule(number, "12345", SkipperBuilder().build());
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, 12345);
  }

  {
    auto document =
        parseTerminalRule(number, "12345x", SkipperBuilder().build());
    auto &result = document->parseResult;
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
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

TEST(TerminalRuleTest, ParseSkipsIgnoredElementsAroundTopLevelRule) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};
  auto skipper = SkipperBuilder().ignore(ws).build();

  auto document = parseTerminalRule(terminal, "abc   ", skipper);
  auto &result = document->parseResult;
  EXPECT_TRUE(result.value);
}

TEST(TerminalRuleTest, BoolConversionMapsTrueAndFalse) {
  TerminalRule<bool> flag{"Flag", "true"_kw | "false"_kw};

  auto document = parseTerminalRule(flag, "true", SkipperBuilder().build());
  auto &yes = document->parseResult;
  ASSERT_TRUE(yes.value);
  auto *yesTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(yes.value);
  ASSERT_TRUE(yesTyped != nullptr);
  EXPECT_TRUE(yesTyped->value);

  document = parseTerminalRule(flag, "false", SkipperBuilder().build());
  auto &no = document->parseResult;
  ASSERT_TRUE(no.value);
  auto *noTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(no.value);
  ASSERT_TRUE(noTyped != nullptr);
  EXPECT_FALSE(noTyped->value);
}

TEST(TerminalRuleTest, FloatingPointConversionAndFailurePaths) {
  TerminalRule<double> number{"Number", some(dot)};

  {
    auto document = parseTerminalRule(number, "12.5", SkipperBuilder().build());
    auto &result = document->parseResult;
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

  auto document = parseTerminalRule(ch, "x", SkipperBuilder().build());
  auto &result = document->parseResult;
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

  auto document = parseTerminalRule(number, "42", SkipperBuilder().build());
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 3);
}

TEST(TerminalRuleTest, GetValueAndParseReturnRuleValueVariant) {
  TerminalRule<std::string_view> rule{"Rule", "abc"_kw};

  auto document = parseTerminalRule(rule, "abc", SkipperBuilder().build());
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  ASSERT_TRUE(parsed.cst != nullptr);

  auto skipper = SkipperBuilder().build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("abc");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, skipper};
  ASSERT_TRUE(parse(rule, ctx));
  auto root = builder.getRootCstNode();
  auto node = root->begin();
  ASSERT_NE(node, root->end());

  auto value = rule.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::string_view>(value));
  EXPECT_EQ(std::get<std::string_view>(value), "abc");
  EXPECT_FALSE(rule.getTypeName().empty());
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

TEST(TerminalRuleTest, BoolConverterMapsNonTrueTextToFalse) {
  TerminalRule<bool> flag{"Flag", some(dot)};

  auto document = parseTerminalRule(flag, "true", SkipperBuilder().build());
  auto &yes = document->parseResult;
  ASSERT_TRUE(yes.value);
  auto *yesTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(yes.value);
  ASSERT_TRUE(yesTyped != nullptr);
  EXPECT_TRUE(yesTyped->value);

  document = parseTerminalRule(flag, "abc", SkipperBuilder().build());
  auto &no = document->parseResult;
  ASSERT_TRUE(no.value);
  auto *noTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(no.value);
  ASSERT_TRUE(noTyped != nullptr);
  EXPECT_FALSE(noTyped->value);
}

TEST(TerminalRuleTest, CanOverrideRule) {
  TerminalRule<bool> rule{"RuleWithOverrides", "a"_kw};
  auto skipper = SkipperBuilder().build();
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "b", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper)->parseResult.fullMatch);

  // first override
  rule = rule.super() | "b"_kw;
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper)->parseResult.fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "b", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper)->parseResult.fullMatch);

  // second override
  rule = rule.super() | "c"_kw;
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper)->parseResult.fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "b", skipper)->parseResult.fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "c", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper)->parseResult.fullMatch);

  // third override
  rule = "d"_kw;
  EXPECT_FALSE(parseTerminalRule(rule, "a", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "b", skipper)->parseResult.fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper)->parseResult.fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "d", skipper)->parseResult.fullMatch);
}

TEST(TerminalRuleTest, NumericAndStringGetValueVariantsMatchRuleType) {
  {
    TerminalRule<std::int8_t> i8{"I8", "12"_kw};
    auto skipper = SkipperBuilder().build();
    auto builderHarness = pegium::test::makeCstBuilderHarness("12");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    ASSERT_TRUE(parse(i8, ctx));
    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    auto value = i8.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<std::int8_t>(value));
    EXPECT_EQ(std::get<std::int8_t>(value), static_cast<std::int8_t>(12));
  }

  {
    TerminalRule<std::uint64_t> u64{"U64", "42"_kw};
    auto skipper = SkipperBuilder().build();
    auto builderHarness = pegium::test::makeCstBuilderHarness("42");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    ASSERT_TRUE(parse(u64, ctx));
    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    auto value = u64.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<std::uint64_t>(value));
    EXPECT_EQ(std::get<std::uint64_t>(value), 42ULL);
  }

  {
    TerminalRule<float> f32{"F32", "3.5"_kw};
    auto skipper = SkipperBuilder().build();
    auto builderHarness = pegium::test::makeCstBuilderHarness("3.5");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    ASSERT_TRUE(parse(f32, ctx));
    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    auto value = f32.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<float>(value));
    EXPECT_FLOAT_EQ(std::get<float>(value), 3.5f);
  }

  {
    TerminalRule<std::string> str{"Str", "abc"_kw};
    auto skipper = SkipperBuilder().build();
    auto builderHarness = pegium::test::makeCstBuilderHarness("abc");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    ASSERT_TRUE(parse(str, ctx));
    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    auto value = str.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<std::string>(value));
    EXPECT_EQ(std::get<std::string>(value), "abc");
  }
}

TEST(TerminalRuleTest, EnumGetValueUsesUnderlyingVariantType) {
  TerminalRule<TerminalMode> mode{"Mode", "m"_kw};
  mode.setValueConverter([](std::string_view) noexcept {
    return opt::conversion_value<TerminalMode>(TerminalMode::Single);
  });

  auto document = parseTerminalRule(mode, "m", SkipperBuilder().build());
  auto &parsed = document->parseResult;
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
