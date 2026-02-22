#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>

using namespace pegium::parser;

TEST(UnorderedGroupTest, ParseTerminalAcceptsAnyOrder) {
  auto group = ":"_kw & ";"_kw;

  {
    std::string_view input = ":;";
    auto result = group.parse_terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 2);
  }

  {
    std::string_view input = ";:";
    auto result = group.parse_terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 2);
  }

  {
    std::string_view input = ":x";
    auto result = group.parse_terminal(input);
    EXPECT_FALSE(result.IsValid());
  }
}

TEST(UnorderedGroupTest, ParseRuleAddsNodesForParsedElements) {
  auto group = ":"_kw & ";"_kw;
  pegium::CstBuilder builder(";:");
  const auto input = builder.getText();
  auto context = ContextBuilder().build();

  ParseState state{builder, context};
  auto result = group.parse_rule(state);
  EXPECT_TRUE(result);
  EXPECT_EQ(state.cursor() - input.begin(), 2);

  auto root = builder.finalize();
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
    std::string_view input = "a";
    auto terminal = group.parse_terminal(input);
    EXPECT_FALSE(terminal.IsValid());
  }

  pegium::CstBuilder builder("a");
  const auto input = builder.getText();
  auto context = ContextBuilder().build();

  ParseState state{builder, context};
  const bool rule = group.parse_rule(state);

  // This is the expected behavior: each element should consume its own span.
  // The current implementation may still validate the same span twice.
  EXPECT_FALSE(rule);
  EXPECT_EQ(state.cursor(), input.begin());
}

TEST(UnorderedGroupTest, ExposesGrammarKind) {
  auto group = ":"_kw & ";"_kw;
  EXPECT_EQ(group.getKind(), pegium::grammar::ElementKind::UnorderedGroup);
}
