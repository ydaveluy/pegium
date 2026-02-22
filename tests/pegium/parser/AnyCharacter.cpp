#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>

using namespace pegium::parser;

TEST(AnyCharacterTest, RejectsEmptyInput) {
  auto result = dot.parse_terminal(std::string_view{});
  EXPECT_FALSE(result.IsValid());
}

TEST(AnyCharacterTest, ConsumesSingleAsciiCharacter) {
  std::string_view input = "abc";
  auto result = dot.parse_terminal(input);

  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 1);
}

TEST(AnyCharacterTest, ConsumesSingleUtf8CodePoint) {
  std::string input{"\xC3\xA9x"}; // "éx"
  std::string_view view = input;
  auto result = dot.parse_terminal(view);

  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - view.begin(), 2);
}

TEST(AnyCharacterTest, RejectsTruncatedAndInvalidUtf8LeadBytes) {
  std::string truncated{"\xC3"};
  std::string_view truncatedView = truncated;
  auto truncatedResult = dot.parse_terminal(truncatedView);
  EXPECT_FALSE(truncatedResult.IsValid());
  EXPECT_EQ(truncatedResult.offset, truncatedView.begin());

  std::string invalidLead{"\x80"};
  std::string_view invalidView = invalidLead;
  auto invalidResult = dot.parse_terminal(invalidView);
  EXPECT_FALSE(invalidResult.IsValid());
  EXPECT_EQ(invalidResult.offset, invalidView.begin());
}

TEST(AnyCharacterTest, ParseRuleAddsNodeAndKeepsCursorOnFailure) {
  auto context = ContextBuilder().build();

  {
    std::string input{"\xC3\xA9x"}; // "éx"
    pegium::CstBuilder builder(input);
    const auto text = builder.getText();

    ParseState state{builder, context};
    auto ok = dot.parse_rule(state);
    EXPECT_TRUE(ok);
    EXPECT_EQ(state.cursor() - text.begin(), 2);

    auto root = builder.finalize();
    auto it = root->begin();
    ASSERT_NE(it, root->end());
    EXPECT_EQ((*it).getText().size(), 2u);
    ++it;
    EXPECT_EQ(it, root->end());
  }

  {
    pegium::CstBuilder builder(std::string{"\xC3"});
    const auto text = builder.getText();

    ParseState state{builder, context};
    auto ko = dot.parse_rule(state);
    EXPECT_FALSE(ko);
    EXPECT_EQ(state.cursor(), text.begin());

    auto root = builder.finalize();
    EXPECT_EQ(root->begin(), root->end());
  }
}

TEST(AnyCharacterTest, ExposesKindAndGrammarValue) {
  EXPECT_EQ(dot.getKind(), pegium::grammar::ElementKind::AnyCharacter);

  pegium::CstBuilder builder("x");
  const char *begin = builder.input_begin();
  builder.leaf(begin, begin + 1, std::addressof(dot), false);
  auto root = builder.finalize();

  auto node = root->begin();
  ASSERT_NE(node, root->end());
  EXPECT_EQ(dot.getValue(*node), "x");
}
