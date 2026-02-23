#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <memory>

using namespace pegium::parser;

TEST(ParserTest, CreateContextReturnsDefaultContext) {
  Parser parser;
  auto context = parser.createContext();

  pegium::CstBuilder builder("");
  const auto input = builder.getText();
  auto result = context.skipHiddenNodes(input, builder);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

TEST(ParserTest, EndOfFileExpressionWorks) {
  auto expr = "a"_kw + eof;

  {
    std::string_view okInput = "a";
    auto ok = expr.terminal(okInput);
    EXPECT_TRUE(ok.IsValid());
    EXPECT_EQ(ok.offset - okInput.begin(), 1);
  }

  {
    std::string_view koInput = "ab";
    auto ko = expr.terminal(koInput);
    EXPECT_FALSE(ko.IsValid());
  }
}

TEST(ParserTest, NegatedCharacterRangeLiteralWorks) {
  auto notNewLine = "^\n"_cr;

  {
    std::string_view okInput = "x";
    auto ok = notNewLine.terminal(okInput);
    EXPECT_TRUE(ok.IsValid());
    EXPECT_EQ(ok.offset - okInput.begin(), 1);
  }

  {
    std::string_view koInput = "\n";
    auto ko = notNewLine.terminal(koInput);
    EXPECT_FALSE(ko.IsValid());
  }
}

TEST(ParserTest, UntilOperatorConsumesUntilClosingToken) {
  auto comment = "/*"_kw <=> "*/"_kw;

  {
    std::string_view okInput = "/*hello*/";
    auto ok = comment.terminal(okInput);
    EXPECT_TRUE(ok.IsValid());
    EXPECT_EQ(ok.offset - okInput.begin(), 9);
  }

  {
    std::string_view koInput = "/*hello";
    auto ko = comment.terminal(koInput);
    EXPECT_FALSE(ko.IsValid());
  }
}

TEST(ParserTest, ParserCanBeDestroyedThroughIParserInterface) {
  std::unique_ptr<IParser> parser = std::make_unique<Parser>();
  ASSERT_TRUE(parser != nullptr);
}

TEST(ParserTest, CustomIParserImplementationCanBeDestroyedPolymorphically) {
  struct DummyParser final : IParser {};

  std::unique_ptr<IParser> parser = std::make_unique<DummyParser>();
  ASSERT_TRUE(parser != nullptr);
}
