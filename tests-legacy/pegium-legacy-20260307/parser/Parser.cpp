#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>
#include <memory>

using namespace pegium::parser;

TEST(ParserTest, NoOpSkipperSkipsNothing) {
  auto context = NoOpSkipper();

  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = context.skip(input.begin(), builder);
  EXPECT_EQ(result, input.begin());
}

TEST(ParserTest, EndOfFileExpressionWorks) {
  auto expr = "a"_kw + eof;

  {
    std::string okInput = "a";
    auto ok = expr.terminal(okInput);
    EXPECT_NE(ok, nullptr);
    EXPECT_EQ(ok - (okInput).c_str(), 1);
  }

  {
    std::string koInput = "ab";
    auto ko = expr.terminal(koInput);
    EXPECT_EQ(ko, nullptr);
  }
}

TEST(ParserTest, NegatedCharacterRangeLiteralWorks) {
  auto notNewLine = "^\n"_cr;

  {
    std::string okInput = "x";
    auto ok = notNewLine.terminal(okInput);
    EXPECT_NE(ok, nullptr);
    EXPECT_EQ(ok - (okInput).c_str(), 1);
  }

  {
    std::string koInput = "\n";
    auto ko = notNewLine.terminal(koInput);
    EXPECT_EQ(ko, nullptr);
  }
}

TEST(ParserTest, UntilOperatorConsumesUntilClosingToken) {
  auto comment = "/*"_kw <=> "*/"_kw;

  {
    std::string okInput = "/*hello*/";
    auto ok = comment.terminal(okInput);
    EXPECT_NE(ok, nullptr);
    EXPECT_EQ(ok - (okInput).c_str(), 9);
  }

  {
    std::string koInput = "/*hello";
    auto ko = comment.terminal(koInput);
    EXPECT_EQ(ko, nullptr);
  }
}

TEST(ParserTest, PegiumParserCanBeDestroyedThroughParserInterface) {
  struct DummyParser final : PegiumParser {
    Rule<pegium::AstNode> Root{"Root", ":"_kw};

    const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
      return Root;
    }
  };

  std::unique_ptr<Parser> parser = std::make_unique<DummyParser>();
  ASSERT_TRUE(parser != nullptr);
}

TEST(ParserTest, CustomParserImplementationCanBeDestroyedPolymorphically) {
  struct DummyParser final : Parser {
    void parse(pegium::workspace::Document &,
               const pegium::utils::CancellationToken & = {}) const override {
    }
  };

  std::unique_ptr<Parser> parser = std::make_unique<DummyParser>();
  ASSERT_TRUE(parser != nullptr);
}
