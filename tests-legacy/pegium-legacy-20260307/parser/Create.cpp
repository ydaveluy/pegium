#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

namespace {

struct ChildNode : pegium::AstNode {};

} // namespace

TEST(CreateTest, CreatesRequestedNodeType) {
  auto makeChild = create<ChildNode>();

  EXPECT_EQ(makeChild.getKind(), pegium::grammar::ElementKind::Create);

  auto created = makeChild.getValue();
  EXPECT_TRUE(pegium::ast_ptr_cast<ChildNode>(created) != nullptr);
}

TEST(CreateTest, ParseMethodsDoNotConsumeInput) {
  auto makeChild = create<ChildNode>();
  std::string input = "abc";

  auto builderHarness = pegium::test::makeCstBuilderHarness(input);
  auto &builder = builderHarness.builder;
  const auto parseInput = builder.getText();
  auto skipper = SkipperBuilder().build();
  ParseContext ctx{builder, skipper};
  auto rule = parse(makeChild, ctx);
  EXPECT_TRUE(rule);
  EXPECT_EQ(ctx.cursor(), parseInput.begin());

  auto root = builder.getRootCstNode();
  std::size_t count = 0;
  for (const auto &child : *root) {
    (void)child;
    ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST(CreateTest, PrintAndTypeNameExposeCreateIntent) {
  auto makeChild = create<ChildNode>();

  EXPECT_FALSE(makeChild.getTypeName().empty());

  std::ostringstream createText;
  createText << makeChild;
  EXPECT_TRUE(!createText.str().empty());
  EXPECT_NE(createText.str().find("ChildNode"), std::string::npos);
}

TEST(CreateTest, CanBeDestroyedThroughGrammarCreatePointer) {
  using CreateExpression = decltype(create<ChildNode>());
  std::unique_ptr<pegium::grammar::Create> createBase =
      std::make_unique<CreateExpression>();
  ASSERT_TRUE(createBase != nullptr);
}
