#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(UnorderedGroupTest, ParseTerminalAcceptsAnyOrder) {
  auto group = ":"_kw & ";"_kw;

  {
    std::string input = ":;";
    auto result = group.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 2);
  }

  {
    std::string input = ";:";
    auto result = group.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 2);
  }

  {
    std::string input = ":x";
    auto result = group.terminal(input);
    EXPECT_EQ(result, nullptr);
  }
}

TEST(UnorderedGroupTest, ParseRuleAddsNodesForParsedElements) {
  auto group = ":"_kw & ";"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness(";:");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(group, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 2);

  auto root = builder.getRootCstNode();
  std::size_t count = 0;
  for (const auto &child : *root) {
    (void)child;
    ++count;
  }
  EXPECT_EQ(count, 2u);
}

TEST(UnorderedGroupTest, ParseRuleRequiresDistinctConsumptionPerElement) {
  auto group = "a"_kw & "a"_kw;

  {
    std::string input = "a";
    auto terminal = group.terminal(input);
    EXPECT_EQ(terminal, nullptr);
  }

  auto builderHarness = pegium::test::makeCstBuilderHarness("a");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  const bool rule = parse(group, ctx);

  // This is the expected behavior: each element should consume its own span.
  // The current implementation may still validate the same span twice.
  EXPECT_FALSE(rule);
  EXPECT_EQ(ctx.cursor(), input.begin());
}

TEST(UnorderedGroupTest, WithLocalSkipperCanMatchInternalSeparators) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto defaultSkipper = SkipperBuilder().build();
  auto group = ("a"_kw & "b"_kw).with_skipper(localSkipper);

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  EXPECT_TRUE(parse(group, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);
}

TEST(UnorderedGroupTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto defaultSkipper = SkipperBuilder().build();
  auto group = ("a"_kw & "b"_kw).with_skipper(localSkipper);
  auto trailing = "c"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b   c");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  ASSERT_TRUE(parse(group, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);

  const auto beforeSkip = ctx.cursorOffset();
  ctx.skip();
  EXPECT_EQ(ctx.cursorOffset(), beforeSkip);
  EXPECT_FALSE(parse(trailing, ctx));
}

TEST(UnorderedGroupTest, WithLocalSkipperDoesNotFlattenAmpersandConcatenation) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto nested = (("a"_kw & "b"_kw).with_skipper(localSkipper)) & "c"_kw;

  std::ostringstream text;
  text << nested;
  EXPECT_EQ(text.str(), "(('a' & 'b') & 'c')");
}

TEST(UnorderedGroupTest, LvalueAmpersandCompositionsAreFlattened) {
  auto left = "a"_kw & "b"_kw;
  auto right = "c"_kw & "d"_kw;
  const auto constRight = "e"_kw & "f"_kw;

  std::ostringstream lhsLvalue;
  lhsLvalue << (left & "e"_kw);
  EXPECT_EQ(lhsLvalue.str(), "('a' & 'b' & 'e')");

  std::ostringstream rhsLvalue;
  rhsLvalue << ("e"_kw & right);
  EXPECT_EQ(rhsLvalue.str(), "('e' & 'c' & 'd')");

  std::ostringstream bothLvalue;
  bothLvalue << (left & right);
  EXPECT_EQ(bothLvalue.str(), "('a' & 'b' & 'c' & 'd')");

  std::ostringstream rvalueLhsConstRhs;
  rvalueLhsConstRhs << (("a"_kw & "b"_kw) & constRight);
  EXPECT_EQ(rvalueLhsConstRhs.str(), "('a' & 'b' & 'e' & 'f')");
}

TEST(UnorderedGroupTest, WithSkipperAmpersandCompositionsStayNestedOnBothSides) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto plain = "c"_kw;
  auto withSkipper = ("a"_kw & "b"_kw).with_skipper(localSkipper);

  std::ostringstream withSkipperLeft;
  withSkipperLeft << (withSkipper & plain);
  EXPECT_EQ(withSkipperLeft.str(), "(('a' & 'b') & 'c')");

  std::ostringstream withSkipperRight;
  withSkipperRight << (plain & withSkipper);
  EXPECT_EQ(withSkipperRight.str(), "('c' & ('a' & 'b'))");
}

TEST(UnorderedGroupTest, ExposesGrammarKind) {
  auto group = ":"_kw & ";"_kw;
  EXPECT_EQ(group.getKind(), pegium::grammar::ElementKind::UnorderedGroup);
}
