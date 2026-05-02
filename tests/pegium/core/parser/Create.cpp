#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
using namespace pegium::parser;

namespace {

struct ChildNode : pegium::AstNode {};

} // namespace

TEST(CreateTest, CreatesRequestedNodeType) {
  auto makeChild = create<ChildNode>();

  EXPECT_EQ(makeChild.getKind(), pegium::grammar::ElementKind::Create);

  pegium::RootCstNode dummyCst{pegium::text::TextSnapshot::copy("")};
  pegium::AstArena arena{dummyCst};
  auto *created = makeChild.getValue(arena);
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
