#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>
#include <variant>

using namespace pegium::parser;

namespace {

struct ChildNode : pegium::AstNode {
  string name;
};

struct AssignmentNode : pegium::AstNode {
  string id;
  vector<string> tags;
  bool enabled = false;
  pointer<ChildNode> child;
};

struct ChoiceNode : pegium::AstNode {
  string op;
};

struct StrictChildNode : pegium::AstNode {
  string name;
};

struct StrictChoiceNode : pegium::AstNode {
  std::variant<bool, std::shared_ptr<StrictChildNode>> value;
};

struct DerivedLeafNode : pegium::AstNode {
  string name;
};

struct AssignmentFeatureNode : pegium::AstNode {
  optional<string> optName;
  reference<DerivedLeafNode> refOne;
  pointer<DerivedLeafNode> child;
  vector<pointer<DerivedLeafNode>> children;
};

struct PointerChoiceNode : pegium::AstNode {
  pointer<DerivedLeafNode> child;
};

struct BoolEnableNode : pegium::AstNode {
  bool enabled = false;
};

struct VariantSourceNode : pegium::AstNode {
  std::variant<bool, std::string> value;
};

struct CharAnyNode : pegium::AstNode {
  std::string value;
};

struct LiteralChoiceNode : pegium::AstNode {
  std::string value;
};

struct PlainLeafValue {
  std::string name;
};

struct DirectValueNode : pegium::AstNode {
  pointer<PlainLeafValue> one;
  vector<pointer<PlainLeafValue>> many;
};

struct NullAssignNode : pegium::AstNode {
  std::nullptr_t marker = nullptr;
};

struct NonConsumingTrueElement final : pegium::grammar::AbstractElement {
  using type = bool;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  void print(std::ostream &os) const override { os << "<true>"; }

  constexpr bool parse_rule(ParseState &) const { return true; }
  bool recover(RecoverState &recoverState) const {
    (void)recoverState;
    return true;
  }
  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *) const noexcept {
    return MatchResult::success(begin);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }
  constexpr bool getValue(const pegium::CstNodeView &) const noexcept {
    return true;
  }
};

struct LeafValueElement final : pegium::grammar::AbstractElement {
  using type = PlainLeafValue;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  void print(std::ostream &os) const override { os << "<leaf-value>"; }

  constexpr bool parse_rule(ParseState &s) const {
    auto result = parse_terminal(s.cursor(), s.end);
    if (!result.IsValid()) {
      return false;
    }
    s.leaf(result.offset, this);
    s.skipHiddenNodes();
    return true;
  }
  bool recover(RecoverState &recoverState) const {
    auto result = parse_terminal(recoverState.cursor(), recoverState.end);
    if (!result.IsValid()) {
      return false;
    }
    recoverState.leaf(result.offset, this);
    recoverState.skipHiddenNodes();
    return true;
  }
  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    constexpr std::string_view token = "leaf";
    if (static_cast<std::size_t>(end - begin) < token.size()) {
      return MatchResult::failure(begin);
    }
    for (std::size_t i = 0; i < token.size(); ++i) {
      if (begin[i] != token[i]) {
        return MatchResult::failure(begin + i);
      }
    }
    return MatchResult::success(begin + token.size());
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }
  type getValue(const pegium::CstNodeView &node) const {
    type value;
    value.name = std::string{node.getText()};
    return value;
  }
};

struct NullValueElement final : pegium::grammar::AbstractElement {
  using type = std::nullptr_t;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  void print(std::ostream &os) const override { os << "<null>"; }

  constexpr bool parse_rule(ParseState &s) const {
    auto result = parse_terminal(s.cursor(), s.end);
    if (!result.IsValid()) {
      return false;
    }
    s.leaf(result.offset, this);
    s.skipHiddenNodes();
    return true;
  }
  bool recover(RecoverState &recoverState) const {
    auto result = parse_terminal(recoverState.cursor(), recoverState.end);
    if (!result.IsValid()) {
      return false;
    }
    recoverState.leaf(result.offset, this);
    recoverState.skipHiddenNodes();
    return true;
  }
  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    constexpr std::string_view token = "nil";
    if (static_cast<std::size_t>(end - begin) < token.size()) {
      return MatchResult::failure(begin);
    }
    for (std::size_t i = 0; i < token.size(); ++i) {
      if (begin[i] != token[i]) {
        return MatchResult::failure(begin + i);
      }
    }
    return MatchResult::success(begin + token.size());
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }
  constexpr type getValue(const pegium::CstNodeView &) const noexcept {
    return nullptr;
  }
};

struct AssignmentParser final : Parser {
  Rule<ChildNode> Child{"Child", assign<&ChildNode::name>("child"_kw)};
  Rule<AssignmentNode> Root{
      "Root",
      assign<&AssignmentNode::id>("id"_kw) + ":"_kw +
          many(append<&AssignmentNode::tags>("tag"_kw), ","_kw) +
          enable_if<&AssignmentNode::enabled>("!"_kw) +
          assign<&AssignmentNode::child>(Child)};
};

struct ChoiceParser final : Parser {
  Rule<ChoiceNode> Root{"Root", assign<&ChoiceNode::op>("+"_kw | "-"_kw)};
};

struct StringViewChoiceParser final : Parser {
  Terminal<std::string_view> Word{"Word", "word"_kw};
  Rule<ChoiceNode> Root{"Root", assign<&ChoiceNode::op>(Word | "-"_kw)};
};

struct StrictOrderedChoiceParser final : Parser {
  Rule<StrictChildNode> Child{"Child", assign<&StrictChildNode::name>("child"_kw)};
  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  Rule<StrictChoiceNode> Root{
      "Root", assign<&StrictChoiceNode::value>(Child | Bool)};
};

struct AssignmentFeatureParser final : Parser {
  Rule<DerivedLeafNode> Child{"Child", assign<&DerivedLeafNode::name>("child"_kw)};
  Rule<AssignmentFeatureNode> Root{
      "Root",
      assign<&AssignmentFeatureNode::optName>("opt"_kw) + ":"_kw +
          assign<&AssignmentFeatureNode::refOne>("ref"_kw) + ":"_kw +
          assign<&AssignmentFeatureNode::child>(Child) + ":"_kw +
          many(append<&AssignmentFeatureNode::children>(Child), ","_kw)};
};

struct PointerChoiceParser final : Parser {
  Rule<DerivedLeafNode> Child{"Child", assign<&DerivedLeafNode::name>("child"_kw)};
  Rule<PointerChoiceNode> Root{
      "Root", assign<&PointerChoiceNode::child>(Child | Child)};
};

struct BoolEnableParser final : Parser {
  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  Rule<BoolEnableNode> Root{
      "Root", enable_if<&BoolEnableNode::enabled>(Bool)};
};

struct VariantSourceParser final : Parser {
  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  DataTypeRule<std::string> Word{"Word", "ab"_kw};
  Rule<VariantSourceNode> Root{"Root", assign<&VariantSourceNode::value>(Bool | Word)};
};

struct CharAnyParser final : Parser {
  Rule<CharAnyNode> Root{"Root", assign<&CharAnyNode::value>(d | dot)};
};

struct LiteralChoiceParser final : Parser {
  Rule<LiteralChoiceNode> Root{
      "Root", assign<&LiteralChoiceNode::value>("z"_kw | "y"_kw)};
};

struct BoolAssignParser final : Parser {
  Rule<BoolEnableNode> Root{"Root", assign<&BoolEnableNode::enabled>("!"_kw)};
};

struct DirectValueParser final : Parser {
  Rule<DirectValueNode> Root{
      "Root",
      assign<&DirectValueNode::one>(LeafValueElement{}) + ":"_kw +
          append<&DirectValueNode::many>(LeafValueElement{})};
};

struct NullValueParser final : Parser {
  Rule<NullAssignNode> Root{
      "Root", assign<&NullAssignNode::marker>(NullValueElement{})};
};

} // namespace

TEST(AssignmentTest, AssignAppendAndEnableIfPopulateAstNode) {
  AssignmentParser parser;

  auto result = parser.Root.parse("id:tag,tag!child", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);

  EXPECT_EQ(result.value->id, "id");
  ASSERT_EQ(result.value->tags.size(), 2u);
  EXPECT_EQ(result.value->tags[0], "tag");
  EXPECT_EQ(result.value->tags[1], "tag");
  EXPECT_TRUE(result.value->enabled);

  ASSERT_TRUE(result.value->child != nullptr);
  EXPECT_EQ(result.value->child->name, "child");
  EXPECT_EQ(result.value->child->getContainer(), result.value.get());
}

TEST(AssignmentTest, OrderedChoiceAssignsMatchedLiteralText) {
  ChoiceParser parser;

  auto plus = parser.Root.parse("+", parser.createContext());
  ASSERT_TRUE(plus.ret);
  ASSERT_TRUE(plus.value != nullptr);
  EXPECT_EQ(plus.value->op, "+");

  auto minus = parser.Root.parse("-", parser.createContext());
  ASSERT_TRUE(minus.ret);
  ASSERT_TRUE(minus.value != nullptr);
  EXPECT_EQ(minus.value->op, "-");
}

TEST(AssignmentTest, OrderedChoiceAssignsStringViewToString) {
  StringViewChoiceParser parser;

  auto word = parser.Root.parse("word", parser.createContext());
  ASSERT_TRUE(word.ret);
  ASSERT_TRUE(word.value != nullptr);
  EXPECT_EQ(word.value->op, "word");

  auto minus = parser.Root.parse("-", parser.createContext());
  ASSERT_TRUE(minus.ret);
  ASSERT_TRUE(minus.value != nullptr);
  EXPECT_EQ(minus.value->op, "-");
}

TEST(AssignmentTest, OrderedChoiceStrictModeDoesNotConvertAstPointerToBool) {
  StrictOrderedChoiceParser parser;

  auto child = parser.Root.parse("child", parser.createContext());
  ASSERT_TRUE(child.ret);
  ASSERT_TRUE(child.value != nullptr);
  EXPECT_TRUE(
      std::holds_alternative<std::shared_ptr<StrictChildNode>>(child.value->value));
  EXPECT_FALSE(std::holds_alternative<bool>(child.value->value));

  auto yes = parser.Root.parse("true", parser.createContext());
  ASSERT_TRUE(yes.ret);
  ASSERT_TRUE(yes.value != nullptr);
  EXPECT_TRUE(std::holds_alternative<bool>(yes.value->value));
  EXPECT_TRUE(std::get<bool>(yes.value->value));
}

TEST(AssignmentTest, OptionalReferenceAndPointerAssignmentsAreHandled) {
  AssignmentFeatureParser parser;

  auto result =
      parser.Root.parse("opt:ref:child:child,child", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);

  ASSERT_TRUE(result.value->optName.has_value());
  EXPECT_EQ(*result.value->optName, "opt");

  ASSERT_TRUE(result.value->child != nullptr);
  EXPECT_EQ(result.value->child->name, "child");
  EXPECT_EQ(result.value->child->getContainer(), result.value.get());

  ASSERT_EQ(result.value->children.size(), 2u);
  ASSERT_TRUE(result.value->children[0] != nullptr);
  ASSERT_TRUE(result.value->children[1] != nullptr);
  EXPECT_EQ(result.value->children[0]->getContainer(), result.value.get());
  EXPECT_EQ(result.value->children[1]->getContainer(), result.value.get());
}

TEST(AssignmentTest, OrderedChoiceParserRuleValueUsesAstPointerPath) {
  PointerChoiceParser parser;

  auto result = parser.Root.parse("child", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  ASSERT_TRUE(result.value->child != nullptr);
  EXPECT_EQ(result.value->child->name, "child");
  EXPECT_EQ(result.value->child->getContainer(), result.value.get());
}

TEST(AssignmentTest, EnableIfWithBoolElementAssignsParsedBooleanValue) {
  BoolEnableParser parser;

  auto yes = parser.Root.parse("true", parser.createContext());
  ASSERT_TRUE(yes.ret);
  ASSERT_TRUE(yes.value != nullptr);
  EXPECT_TRUE(yes.value->enabled);

  auto no = parser.Root.parse("false", parser.createContext());
  ASSERT_TRUE(no.ret);
  ASSERT_TRUE(no.value != nullptr);
  EXPECT_FALSE(no.value->enabled);
}

TEST(AssignmentTest, OrderedChoiceSupportsTerminalAndDataTypeKinds) {
  VariantSourceParser parser;

  {
    auto result = parser.Root.parse("true", parser.createContext());
    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.value != nullptr);
    EXPECT_TRUE(std::holds_alternative<bool>(result.value->value));
    EXPECT_TRUE(std::get<bool>(result.value->value));
  }

  {
    auto result = parser.Root.parse("ab", parser.createContext());
    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.value != nullptr);
    EXPECT_TRUE(std::holds_alternative<std::string>(result.value->value));
    EXPECT_EQ(std::get<std::string>(result.value->value), "ab");
  }

}

TEST(AssignmentTest, OrderedChoiceSupportsLiteralKindWithStringFallback) {
  LiteralChoiceParser parser;

  auto result = parser.Root.parse("z", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  EXPECT_EQ(result.value->value, "z");
}

TEST(AssignmentTest, OrderedChoiceSupportsCharacterRangeAndAnyCharacterKinds) {
  CharAnyParser parser;

  {
    auto result = parser.Root.parse("7", parser.createContext());
    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.value != nullptr);
    EXPECT_EQ(result.value->value, "7");
  }

  {
    auto result = parser.Root.parse(":", parser.createContext());
    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.value != nullptr);
    EXPECT_EQ(result.value->value, ":");
  }
}

TEST(AssignmentTest, ParseRuleForNonOrderedOverridesGrammarElement) {
  auto expression = assign<&AssignmentNode::id>("id"_kw);
  auto context = ContextBuilder().build();

  {
    pegium::CstBuilder builder("id");
    const auto input = builder.getText();
    ParseState state{builder, context};

    auto ok = expression.parse_rule(state);
    EXPECT_TRUE(ok);
    EXPECT_EQ(state.cursor() - input.begin(), 2);

    auto root = builder.finalize();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(expression));
  }

  {
    pegium::CstBuilder builder("xx");
    ParseState state{builder, context};
    auto ko = expression.parse_rule(state);
    EXPECT_FALSE(ko);

    auto root = builder.finalize();
    EXPECT_EQ(root->begin(), root->end());
  }
}

TEST(AssignmentTest, ParseRuleForOrderedChoiceRewindsOnFailure) {
  auto expression = assign<&ChoiceNode::op>("+"_kw | "-"_kw);
  auto context = ContextBuilder().build();

  {
    pegium::CstBuilder builder("+");
    const auto input = builder.getText();
    ParseState state{builder, context};

    auto ok = expression.parse_rule(state);
    EXPECT_TRUE(ok);
    EXPECT_EQ(state.cursor() - input.begin(), 1);

    auto root = builder.finalize();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(expression));
  }

  {
    pegium::CstBuilder builder("*");
    const auto input = builder.getText();
    ParseState state{builder, context};
    auto ko = expression.parse_rule(state);
    EXPECT_FALSE(ko);
    EXPECT_EQ(state.cursor(), input.begin());

    auto root = builder.finalize();
    EXPECT_EQ(root->begin(), root->end());
  }
}

TEST(AssignmentTest, ParseRuleForNonOrderedWithoutNodeProductionKeepsTreeEmpty) {
  NonConsumingTrueElement element;
  auto expression = assign<&BoolEnableNode::enabled>(element);
  auto context = ContextBuilder().build();

  pegium::CstBuilder builder("");
  const auto input = builder.getText();
  ParseState state{builder, context};

  auto ok = expression.parse_rule(state);
  EXPECT_TRUE(ok);
  EXPECT_EQ(state.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AssignmentTest, AssignOnBoolWithNonBoolElementSetsTrue) {
  BoolAssignParser parser;

  auto result = parser.Root.parse("!", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  EXPECT_TRUE(result.value->enabled);
}

TEST(AssignmentTest, AssignmentMetadataAndOperatorsAreExposed) {
  auto assignExpr = assign<&AssignmentNode::id>("id"_kw);
  auto appendExpr = append<&AssignmentNode::tags>("tag"_kw);
  auto enableExpr = enable_if<&AssignmentNode::enabled>("!"_kw);

  EXPECT_EQ(assignExpr.getKind(), pegium::grammar::ElementKind::Assignment);
  EXPECT_EQ(assignExpr.getOperator(), pegium::grammar::AssignmentOperator::Assign);
  EXPECT_EQ(appendExpr.getOperator(), pegium::grammar::AssignmentOperator::Append);
  EXPECT_EQ(enableExpr.getOperator(), pegium::grammar::AssignmentOperator::EnableIf);

  std::ostringstream opText;
  opText << pegium::grammar::AssignmentOperator::Assign
         << pegium::grammar::AssignmentOperator::Append
         << pegium::grammar::AssignmentOperator::EnableIf;
  EXPECT_EQ(opText.str(), "=+=?=");

  std::ostringstream assignmentText;
  assignmentText << assignExpr;
  EXPECT_NE(assignmentText.str().find("id="), std::string::npos);
}

TEST(AssignmentTest, DirectObjectValuesAssignAndAppendSharedPointers) {
  DirectValueParser parser;

  auto result = parser.Root.parse("leaf:leaf", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);

  ASSERT_TRUE(result.value->one != nullptr);
  EXPECT_EQ(result.value->one->name, "leaf");

  ASSERT_EQ(result.value->many.size(), 1u);
  ASSERT_TRUE(result.value->many[0] != nullptr);
  EXPECT_EQ(result.value->many[0]->name, "leaf");
}

TEST(AssignmentTest, NullptrValueAssignmentIsAccepted) {
  NullValueParser parser;

  auto result = parser.Root.parse("nil", parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  EXPECT_EQ(result.value->marker, nullptr);
}

TEST(AssignmentTest, OrderedChoiceExecuteThrowsWhenSelectedChildIsInvalid) {
  auto expression = assign<&LiteralChoiceNode::value>("z"_kw | "y"_kw);

  pegium::CstBuilder builder("z");
  const char *begin = builder.input_begin();
  builder.enter(begin);
  builder.leaf(begin, begin + 1, nullptr, false);
  builder.exit(begin + 1, std::addressof(expression));
  auto root = builder.finalize();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  LiteralChoiceNode current;
  EXPECT_THROW(expression.execute(&current, *node), std::runtime_error);
}

TEST(AssignmentTest, OrderedChoiceExecuteThrowsWhenSelectedValueIsNotAssignable) {
  auto expression = assign<&ChoiceNode::op>("+"_kw | "-"_kw);
  ParserRule<StrictChildNode> childRule{
      "Child", assign<&StrictChildNode::name>("child"_kw)};

  pegium::CstBuilder builder("child");
  const char *begin = builder.input_begin();
  builder.enter(begin);
  builder.leaf(begin, begin + 5, std::addressof(childRule), false);
  builder.exit(begin + 5, std::addressof(expression));
  auto root = builder.finalize();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  ChoiceNode current;
  EXPECT_THROW(expression.execute(&current, *node), std::runtime_error);
}

TEST(AssignmentTest, OrderedChoiceExecuteThrowsWhenOnlyHiddenChildrenExist) {
  auto expression = assign<&LiteralChoiceNode::value>("z"_kw | "y"_kw);
  const auto hiddenToken = " "_kw;

  pegium::CstBuilder builder(" ");
  const char *begin = builder.input_begin();
  builder.enter(begin);
  builder.leaf(begin, begin + 1, std::addressof(hiddenToken), true);
  builder.exit(begin + 1, std::addressof(expression));
  auto root = builder.finalize();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  LiteralChoiceNode current;
  EXPECT_THROW(expression.execute(&current, *node), std::runtime_error);
}

TEST(AssignmentTest, OrderedChoiceExecuteSkipsHiddenChildren) {
  auto expression = assign<&LiteralChoiceNode::value>("z"_kw | "y"_kw);
  const auto hiddenToken = " "_kw;
  const auto visibleToken = "z"_kw;

  pegium::CstBuilder builder("z");
  const char *begin = builder.input_begin();
  builder.enter(begin);
  builder.leaf(begin, begin, std::addressof(hiddenToken), true);
  builder.leaf(begin, begin + 1, std::addressof(visibleToken), false);
  builder.exit(begin + 1, std::addressof(expression));
  auto root = builder.finalize();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  LiteralChoiceNode current;
  expression.execute(&current, *node);
  EXPECT_EQ(current.value, "z");
}
