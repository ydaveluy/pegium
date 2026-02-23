#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <pegium/parser/Parser.hpp>
#include <variant>

using namespace pegium::parser;

TEST(TerminalRuleTest, ParseRequiresFullConsumption) {
  TerminalRule<std::string_view> terminal{"T", "hello"_kw};

  {
    auto result = terminal.parse("hello", SkipperBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_EQ(result.len, 5u);
    EXPECT_EQ(result.value, "hello");
  }

  {
    auto result = terminal.parse("helloX", SkipperBuilder().build());
    EXPECT_FALSE(result.ret);
    EXPECT_EQ(result.len, 5u);
  }
}

TEST(TerminalRuleTest, IntegralConversionUsesFromChars) {
  TerminalRule<int> number{"Number", some(d)};

  {
    auto result = number.parse("12345", SkipperBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_EQ(result.value, 12345);
  }

  {
    auto result = number.parse("12345x", SkipperBuilder().build());
    EXPECT_FALSE(result.ret);
    EXPECT_EQ(result.len, 5u);
  }
}

TEST(TerminalRuleTest, ParseRuleSkipsIgnoredElementsAfterToken) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};

  pegium::CstBuilder builder("abc   ");
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().ignore(ws).build();

  ParseContext ctx{builder, skipper};
  auto result = terminal.rule(ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 6);

  auto root = builder.finalize();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), "abc");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(TerminalRuleTest, BoolConversionMapsTrueAndFalse) {
  TerminalRule<bool> flag{"Flag", "true"_kw | "false"_kw};

  auto yes = flag.parse("true", SkipperBuilder().build());
  ASSERT_TRUE(yes.ret);
  EXPECT_TRUE(yes.value);

  auto no = flag.parse("false", SkipperBuilder().build());
  ASSERT_TRUE(no.ret);
  EXPECT_FALSE(no.value);
}

TEST(TerminalRuleTest, FloatingPointConversionAndFailurePaths) {
  TerminalRule<double> number{"Number", some(dot)};

  {
    auto result = number.parse("12.5", SkipperBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_DOUBLE_EQ(result.value, 12.5);
  }

  /*EXPECT_THROW((void)number.parse("abc", SkipperBuilder().build()),
               std::invalid_argument);
  EXPECT_THROW((void)number.parse("12abc", SkipperBuilder().build()),
               std::invalid_argument);*/
  // TODO check it returns 0.0 instead of throwing, and that the result is
  // marked as recovered.
}

/*TEST(TerminalRuleTest, CharRuleRequiresConverterButCanUseCustomConverter) {
  TerminalRule<char> ch{"Char", "x"_kw};
  EXPECT_THROW((void)ch.parse("x", SkipperBuilder().build()), std::logic_error);

  ch.setValueConverter([](std::string_view sv) -> char {
    return sv.empty() ? '\0' : sv.front();
  });

  auto result = ch.parse("x", SkipperBuilder().build());
  ASSERT_TRUE(result.ret);
  EXPECT_EQ(result.value, 'x');
}*/

TEST(TerminalRuleTest, GetValueAndParseGenericReturnRuleValueVariant) {
  TerminalRule<std::string_view> rule{"Rule", "abc"_kw};

  auto parsed = rule.parse("abc", SkipperBuilder().build());
  ASSERT_TRUE(parsed.ret);
  ASSERT_TRUE(parsed.root_node != nullptr);

  auto node =
      detail::findFirstMatchingNode(*parsed.root_node, std::addressof(rule));
  ASSERT_TRUE(node.has_value());

  auto value = rule.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::string_view>(value));
  EXPECT_EQ(std::get<std::string_view>(value), "abc");
  EXPECT_FALSE(rule.getTypeName().empty());

  auto generic = rule.parseGeneric("abc", SkipperBuilder().build());
  ASSERT_TRUE(generic.root_node != nullptr);
}

/*TEST(TerminalRuleTest, ParseRuleFailureLeavesCursorAndTreeUntouched) {
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};
  pegium::CstBuilder builder("abX");
  const auto input = builder.getText();
  auto skipper = ContextBuilder().build();

  RecoverState ctx{builder, skipper};
  auto result = terminal.rule(ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}*/

TEST(TerminalRuleTest, BoolConverterMapsNonTrueTextToFalse) {
  TerminalRule<bool> flag{"Flag", some(dot)};

  auto yes = flag.parse("true", SkipperBuilder().build());
  ASSERT_TRUE(yes.ret);
  EXPECT_TRUE(yes.value);

  auto no = flag.parse("abc", SkipperBuilder().build());
  ASSERT_TRUE(no.ret);
  EXPECT_FALSE(no.value);
}

TEST(TerminalRuleTest, CanOverrideRule) {
  TerminalRule<bool> rule{"RuleWithOverrides", "a"_kw};
  auto skipper = SkipperBuilder().build();
  EXPECT_TRUE(rule.parse("a", skipper).ret);
  EXPECT_FALSE(rule.parse("b", skipper).ret);
  EXPECT_FALSE(rule.parse("c", skipper).ret);
  EXPECT_FALSE(rule.parse("d", skipper).ret);

  // first override
  rule = rule.super() | "b"_kw;
  EXPECT_TRUE(rule.parse("a", skipper).ret);
  EXPECT_TRUE(rule.parse("b", skipper).ret);
  EXPECT_FALSE(rule.parse("c", skipper).ret);
  EXPECT_FALSE(rule.parse("d", skipper).ret);

  // second override
  rule = rule.super() | "c"_kw;
  EXPECT_TRUE(rule.parse("a", skipper).ret);
  EXPECT_TRUE(rule.parse("b", skipper).ret);
  EXPECT_TRUE(rule.parse("c", skipper).ret);
  EXPECT_FALSE(rule.parse("d", skipper).ret);

  // third override
  rule = "d"_kw;
  EXPECT_FALSE(rule.parse("a", skipper).ret);
  EXPECT_FALSE(rule.parse("b", skipper).ret);
  EXPECT_FALSE(rule.parse("c", skipper).ret);
  EXPECT_TRUE(rule.parse("d", skipper).ret);
}

TEST(TerminalRuleTest, NumericAndStringGetValueVariantsMatchRuleType) {
  auto skipper = SkipperBuilder().build();

  {
    TerminalRule<std::int8_t> i8{"I8", "12"_kw};
    auto parsed = i8.parse("12", skipper);
    ASSERT_TRUE(parsed.ret);
    auto node =
        detail::findFirstMatchingNode(*parsed.root_node, std::addressof(i8));
    ASSERT_TRUE(node.has_value());
    auto value = i8.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<std::int8_t>(value));
    EXPECT_EQ(std::get<std::int8_t>(value), static_cast<std::int8_t>(12));
  }

  {
    TerminalRule<std::uint64_t> u64{"U64", "42"_kw};
    auto parsed = u64.parse("42", skipper);
    ASSERT_TRUE(parsed.ret);
    auto node =
        detail::findFirstMatchingNode(*parsed.root_node, std::addressof(u64));
    ASSERT_TRUE(node.has_value());
    auto value = u64.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<std::uint64_t>(value));
    EXPECT_EQ(std::get<std::uint64_t>(value), 42ULL);
  }

  {
    TerminalRule<float> f32{"F32", "3.5"_kw};
    auto parsed = f32.parse("3.5", skipper);
    ASSERT_TRUE(parsed.ret);
    auto node =
        detail::findFirstMatchingNode(*parsed.root_node, std::addressof(f32));
    ASSERT_TRUE(node.has_value());
    auto value = f32.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<float>(value));
    EXPECT_FLOAT_EQ(std::get<float>(value), 3.5f);
  }

  {
    TerminalRule<std::string> str{"Str", "abc"_kw};
    auto parsed = str.parse("abc", skipper);
    ASSERT_TRUE(parsed.ret);
    auto node =
        detail::findFirstMatchingNode(*parsed.root_node, std::addressof(str));
    ASSERT_TRUE(node.has_value());
    auto value = str.getValue(*node);
    EXPECT_TRUE(std::holds_alternative<std::string>(value));
    EXPECT_EQ(std::get<std::string>(value), "abc");
  }
}
