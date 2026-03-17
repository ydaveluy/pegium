#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>

using namespace pegium::parser;

TEST(AnyCharacterTest, RejectsEmptyInput) {
  auto result = dot.terminal(std::string{});
  EXPECT_EQ(result, nullptr);
}

TEST(AnyCharacterTest, RejectsSentinelNul) {
  std::string nulInput{"\0", 1};
  auto result = dot.terminal(nulInput);
  EXPECT_EQ(result, nullptr);
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

TEST(AnyCharacterTest, RejectsTruncatedAndInvalidUtf8LeadBytes) {
  std::string truncated{"\xC3"};
  auto truncatedResult = dot.terminal(truncated);
  EXPECT_EQ(truncatedResult, nullptr);

  std::string invalidLead{"\x80"};
  auto invalidResult = dot.terminal(invalidLead);
  EXPECT_EQ(invalidResult, nullptr);
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

TEST(AnyCharacterTest, ExposesKindAndGrammarValue) {
  EXPECT_EQ(dot.getKind(), pegium::grammar::ElementKind::AnyCharacter);

  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  const char *begin = builder.input_begin();
  builder.leaf(begin, begin + 1, std::addressof(dot), false);
  auto root = builder.getRootCstNode();

  auto node = root->begin();
  ASSERT_NE(node, root->end());
  EXPECT_EQ(dot.getValue(*node), "x");
}
