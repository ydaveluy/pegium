#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

namespace {

struct ChildNode : pegium::AstNode {};

struct ParentNode : pegium::AstNode {
  pointer<ChildNode> child;
};

} // namespace

TEST(ActionTest, NewActionCreatesRequestedNodeType) {
  auto makeChild = action<ChildNode>();

  EXPECT_EQ(makeChild.getKind(), pegium::grammar::ElementKind::New);

  auto created = makeChild.execute(nullptr);
  EXPECT_TRUE(std::dynamic_pointer_cast<ChildNode>(created) != nullptr);
}

TEST(ActionTest, InitActionAssignsCurrentValueToFeature) {
  auto initParent = action<&ParentNode::child>();
  auto current = std::make_shared<ChildNode>();

  EXPECT_EQ(initParent.getKind(), pegium::grammar::ElementKind::Init);

  auto created = initParent.execute(current);
  auto parent = std::dynamic_pointer_cast<ParentNode>(created);

  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(parent->child != nullptr);
  EXPECT_EQ(parent->child.get(), current.get());
  EXPECT_EQ(parent->child->getContainer(), parent.get());
}

TEST(ActionTest, ParseMethodsDoNotConsumeInput) {
  auto makeChild = action<ChildNode>();
  std::string_view input = "abc";

  auto terminal = makeChild.terminal(input);
  EXPECT_TRUE(terminal.IsValid());
  EXPECT_EQ(terminal.offset, input.begin());

  pegium::CstBuilder builder(input);
  const auto parseInput = builder.getText();
  auto skipper = SkipperBuilder().build();
  ParseContext ctx{builder, skipper};
  auto rule = makeChild.rule(ctx);
  EXPECT_TRUE(rule);
  EXPECT_EQ(ctx.cursor(), parseInput.begin());

  auto root = builder.finalize();
  std::size_t count = 0;
  for (const auto &child : *root) {
    (void)child;
    ++count;
  }
  EXPECT_EQ(count, 1u);
}

TEST(ActionTest, PrintAndTypeNameExposeActionIntent) {
  auto makeChild = action<ChildNode>();
  auto initParent = action<&ParentNode::child>();

  EXPECT_FALSE(makeChild.getTypeName().empty());
  EXPECT_FALSE(initParent.getTypeName().empty());

  std::ostringstream newText;
  newText << makeChild;
  EXPECT_NE(newText.str().find("new"), std::string::npos);
  EXPECT_NE(newText.str().find("ChildNode"), std::string::npos);

  std::ostringstream initText;
  initText << initParent;
  EXPECT_NE(initText.str().find("current"), std::string::npos);
  EXPECT_NE(initText.str().find("ParentNode"), std::string::npos);
}

TEST(ActionTest, ExplicitFeatureTemplateActionAssignsCurrentValue) {
  auto initParent = action<ParentNode, &ParentNode::child>();
  auto current = std::make_shared<ChildNode>();

  auto created = initParent.execute(current);
  auto parent = std::dynamic_pointer_cast<ParentNode>(created);

  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(parent->child != nullptr);
  EXPECT_EQ(parent->child.get(), current.get());
  EXPECT_EQ(parent->child->getContainer(), parent.get());
}

TEST(ActionTest, CanBeDestroyedThroughGrammarActionPointer) {
  using NewAction = decltype(action<ChildNode>());
  std::unique_ptr<pegium::grammar::Action> actionBase =
      std::make_unique<NewAction>();
  ASSERT_TRUE(actionBase != nullptr);
}
