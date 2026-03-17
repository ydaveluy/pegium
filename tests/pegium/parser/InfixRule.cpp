#include <gtest/gtest.h>
#include <pegium/ParseJsonTestSupport.hpp>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/ParseAttempt.hpp>
#include <pegium/parser/PegiumParser.hpp>

using namespace pegium::parser;

namespace {

struct Expr : pegium::AstNode {};

struct LiteralExpr : Expr {
  string name;
};

struct BinaryExpr : Expr {
  pointer<Expr> left;
  string op;
  pointer<Expr> right;
};

enum class BinaryOp : std::uint8_t {
  Plus = 1,
  Minus = 2,
};

struct EnumBinaryExpr : Expr {
  pointer<Expr> left;
  BinaryOp op = BinaryOp::Plus;
  pointer<Expr> right;
};

struct InfixParser final : PegiumParser {
  Terminal<std::string> ID{"ID", "a-z"_cr + many(w)};
  Rule<Expr> Primary{
      "Primary", create<LiteralExpr>() + assign<&LiteralExpr::name>(ID)};
  Infix<BinaryExpr, &BinaryExpr::left, &BinaryExpr::op, &BinaryExpr::right>
      Binary{"Binary", Primary, LeftAssociation("+"_kw | "-"_kw)};
  Rule<Expr> Root{"Root", Binary};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct EnumInfixParser final : PegiumParser {
  Terminal<std::string> ID{"ID", "a-z"_cr + many(w)};
  Terminal<BinaryOp> OP{
      "OP", "+"_kw | "-"_kw,
      opt::with_converter([](std::string_view text) noexcept
                              -> opt::ConversionResult<BinaryOp> {
        return opt::conversion_value<BinaryOp>(
            text == "+" ? BinaryOp::Plus : BinaryOp::Minus);
      })};
  Rule<Expr> Primary{
      "Primary", create<LiteralExpr>() + assign<&LiteralExpr::name>(ID)};
  Infix<EnumBinaryExpr, &EnumBinaryExpr::left, &EnumBinaryExpr::op,
        &EnumBinaryExpr::right>
      Binary{"Binary", Primary, LeftAssociation(OP)};
  Rule<Expr> Root{"Root", Binary};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

} // namespace

TEST(InfixRuleTest, BuildsTypedBinaryExpressionAndSetsContainers) {
  InfixParser parser;
  auto document = pegium::test::ParseDocument(parser, "a+b");

  auto *binary = pegium::ast_ptr_cast<BinaryExpr>(document->parseResult.value);
  ASSERT_TRUE(binary != nullptr);
  ASSERT_TRUE(binary->left != nullptr);
  ASSERT_TRUE(binary->right != nullptr);
  EXPECT_EQ(binary->op, "+");
  EXPECT_EQ(binary->left->getContainer(), binary);
  EXPECT_EQ(binary->left->getContainerPropertyName(), "left");
  EXPECT_EQ(binary->right->getContainer(), binary);
  EXPECT_EQ(binary->right->getContainerPropertyName(), "right");

  auto *left = dynamic_cast<LiteralExpr *>(binary->left.get());
  auto *right = dynamic_cast<LiteralExpr *>(binary->right.get());
  ASSERT_TRUE(left != nullptr);
  ASSERT_TRUE(right != nullptr);
  EXPECT_EQ(left->name, "a");
  EXPECT_EQ(right->name, "b");
}

TEST(InfixRuleTest, OrderedChoiceOperatorKeepsCompileTimeTypedRecursion) {
  InfixParser parser;
  auto document = pegium::test::ParseDocument(parser, "a-b-c");

  auto *binary = pegium::ast_ptr_cast<BinaryExpr>(document->parseResult.value);
  ASSERT_TRUE(binary != nullptr);
  EXPECT_EQ(binary->op, "-");

  auto *leftBinary = dynamic_cast<BinaryExpr *>(binary->left.get());
  auto *rightLeaf = dynamic_cast<LiteralExpr *>(binary->right.get());
  ASSERT_TRUE(leftBinary != nullptr);
  ASSERT_TRUE(rightLeaf != nullptr);
  EXPECT_EQ(rightLeaf->name, "c");
  EXPECT_EQ(binary->right->getContainer(), binary);

  EXPECT_EQ(leftBinary->op, "-");
  auto *leftLeaf = dynamic_cast<LiteralExpr *>(leftBinary->left.get());
  auto *middleLeaf = dynamic_cast<LiteralExpr *>(leftBinary->right.get());
  ASSERT_TRUE(leftLeaf != nullptr);
  ASSERT_TRUE(middleLeaf != nullptr);
  EXPECT_EQ(leftLeaf->name, "a");
  EXPECT_EQ(middleLeaf->name, "b");
  EXPECT_EQ(leftBinary->getContainer(), binary);
  EXPECT_EQ(leftBinary->getContainerPropertyName(), "left");
}

TEST(InfixRuleTest, EnumOperatorIsAssignedFromTypedTerminalRawValue) {
  EnumInfixParser parser;
  auto document = pegium::test::ParseDocument(parser, "a-b");

  auto *binary =
      pegium::ast_ptr_cast<EnumBinaryExpr>(document->parseResult.value);
  ASSERT_TRUE(binary != nullptr);
  EXPECT_EQ(binary->op, BinaryOp::Minus);
  ASSERT_TRUE(binary->left != nullptr);
  ASSERT_TRUE(binary->right != nullptr);

  auto *left = dynamic_cast<LiteralExpr *>(binary->left.get());
  auto *right = dynamic_cast<LiteralExpr *>(binary->right.get());
  ASSERT_TRUE(left != nullptr);
  ASSERT_TRUE(right != nullptr);
  EXPECT_EQ(left->name, "a");
  EXPECT_EQ(right->name, "b");
}

TEST(InfixRuleTest, StrictSafeOperatorProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto op = LeftAssociation("+"_kw | "-"_kw);
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("+");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_TRUE(probe(op, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("x");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_FALSE(probe(op, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}

TEST(InfixRuleTest, FastProbeMatchesStrictParseOutcome) {
  auto op = LeftAssociation("+"_kw | "-"_kw);
  auto skipper = SkipperBuilder().build();

  {
    auto fastHarness = pegium::test::makeCstBuilderHarness("+");
    auto &fastBuilder = fastHarness.builder;
    ParseContext fastCtx{fastBuilder, skipper};

    auto parseHarness = pegium::test::makeCstBuilderHarness("+");
    auto &parseBuilder = parseHarness.builder;
    ParseContext parseCtx{parseBuilder, skipper};

    EXPECT_EQ(attempt_fast_probe(fastCtx, op), parse(op, parseCtx));
  }

  {
    auto fastHarness = pegium::test::makeCstBuilderHarness("x");
    auto &fastBuilder = fastHarness.builder;
    ParseContext fastCtx{fastBuilder, skipper};

    auto parseHarness = pegium::test::makeCstBuilderHarness("x");
    auto &parseBuilder = parseHarness.builder;
    ParseContext parseCtx{parseBuilder, skipper};

    EXPECT_EQ(attempt_fast_probe(fastCtx, op), parse(op, parseCtx));
  }
}
