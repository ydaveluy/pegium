#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

using namespace pegium::parser;

TEST(AnyCharacterTest, RejectsInput) {
  struct Case {
    const char *name;
    std::string input;
  };
  const Case kCases[] = {
      {"RejectsEmptyInput", std::string{}},
      {"RejectsSentinelNul", std::string{"\0", 1}},
      {"RejectsTruncatedUtf8LeadByte", std::string{"\xC3"}},
      {"RejectsInvalidUtf8LeadByte", std::string{"\x80"}},
  };
  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);
    auto result = dot.terminal(c.input);
    EXPECT_EQ(result, nullptr);
  }
}

TEST(AnyCharacterTest, ConsumesSingleAsciiCharacter) {
  std::string input = "abc";
  auto result = dot.terminal(input);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 1);
}

TEST(AnyCharacterTest, ConsumesSingleUtf8CodePoint) {
  std::string input{"\xC3\xA9x"}; // "éx"
  auto result = dot.terminal(input);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 2);
}

TEST(AnyCharacterTest, ParseRuleAddsNodeAndKeepsCursorOnFailure) {
  auto context = SkipperBuilder().build();

  {
    std::string input{"\xC3\xA9x"}; // "éx"
    auto builderHarness = pegium::test::makeCstBuilderHarness(input);
    auto &builder = builderHarness.builder;
    const auto text = builder.getText();

    ParseContext state{builder, context};
    auto ok = parse(dot, state);
    EXPECT_TRUE(ok);
    EXPECT_EQ(state.cursor() - text.begin(), 2);

    auto root = builder.getRootCstNode();
    auto it = root->begin();
    ASSERT_NE(it, root->end());
    EXPECT_EQ((*it).getText().size(), 2u);
    ++it;
    EXPECT_EQ(it, root->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(std::string{"\xC3"});
    auto &builder = builderHarness.builder;
    const auto text = builder.getText();

    ParseContext state{builder, context};
    auto ko = parse(dot, state);
    EXPECT_FALSE(ko);
    EXPECT_EQ(state.cursor(), text.begin());

    auto root = builder.getRootCstNode();
    EXPECT_EQ(root->begin(), root->end());
  }
}

TEST(AnyCharacterTest, StrictProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto skipper = SkipperBuilder().build();

  {
    std::string input{"\xC3\xA9x"}; // "éx"
    auto builderHarness = pegium::test::makeCstBuilderHarness(input);
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_TRUE(probe(dot, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(std::string{"\xC3"});
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_FALSE(probe(dot, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}
