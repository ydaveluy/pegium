#include <cstdint>
#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>
#include <string>
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
  std::variant<bool, pointer<StrictChildNode>> value;
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

enum class AssignMode : std::uint8_t {
  Alpha = 1,
  Beta = 2,
};

struct EnumChoiceNode : pegium::AstNode {
  AssignMode mode = AssignMode::Alpha;
  optional<AssignMode> maybeMode;
  std::variant<AssignMode, std::string> variantMode;
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
  static constexpr bool nullable = true;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<true>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return terminal(text.c_str());
  }
  constexpr bool getValue(const pegium::CstNodeView &) const noexcept {
    return true;
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &ctx) const {
    (void)ctx;
    return true;
  }
};

struct LeafValueElement final : pegium::grammar::AbstractElement {
  using type = PlainLeafValue;
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<leaf-value>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    constexpr std::string_view token = "leaf";
    for (std::size_t i = 0; i < token.size(); ++i) {
      if (begin[i] == '\0') {
        return nullptr;
      }
    }
    for (std::size_t i = 0; i < token.size(); ++i) {
      if (begin[i] != token[i]) {
        return nullptr;
      }
    }
    return begin + token.size();
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return terminal(text.c_str());
  }
  type getValue(const pegium::CstNodeView &node) const {
    type value;
    value.name = std::string{node.getText()};
    return value;
  }
  type getRawValue(const pegium::CstNodeView &node) const {
    return getValue(node);
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &ctx) const {
    if constexpr (std::same_as<std::remove_cvref_t<Context>, ExpectContext>) {
      return true;
    } else {
      auto result = terminal(ctx.cursor());
      if (result == nullptr) {
        return false;
      }
      ctx.leaf(result, this);
      ctx.skip();
      return true;
    }
  }
};

struct NullValueElement final : pegium::grammar::AbstractElement {
  using type = std::nullptr_t;

  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  void print(std::ostream &os) const override { os << "<null>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    constexpr std::string_view token = "nil";
    for (std::size_t i = 0; i < token.size(); ++i) {
      if (begin[i] == '\0') {
        return nullptr;
      }
    }
    for (std::size_t i = 0; i < token.size(); ++i) {
      if (begin[i] != token[i]) {
        return nullptr;
      }
    }
    return begin + token.size();
  }
  constexpr const char *terminal(const std::string &text) const noexcept {
    return terminal(text.c_str());
  }
  constexpr type getValue(const pegium::CstNodeView &) const noexcept {
    return nullptr;
  }
  constexpr type getRawValue(const pegium::CstNodeView &) const noexcept {
    return nullptr;
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  template <typename Context> bool parse_impl(Context &ctx) const {
    if constexpr (std::same_as<std::remove_cvref_t<Context>, ExpectContext>) {
      return true;
    } else {
      auto result = terminal(ctx.cursor());
      if (result == nullptr) {
        return false;
      }
      ctx.leaf(result, this);
      ctx.skip();
      return true;
    }
  }
};

struct AssignmentParser final : PegiumParser {
  Rule<ChildNode> Child{"Child", assign<&ChildNode::name>("child"_kw)};
  Rule<AssignmentNode> Root{
      "Root", assign<&AssignmentNode::id>("id"_kw) + ":"_kw +
                  many(append<&AssignmentNode::tags>("tag"_kw), ","_kw) +
                  enable_if<&AssignmentNode::enabled>("!"_kw) +
                  assign<&AssignmentNode::child>(Child)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct ChoiceParser final : PegiumParser {
  Rule<ChoiceNode> Root{"Root", assign<&ChoiceNode::op>("+"_kw | "-"_kw)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct StringViewChoiceParser final : PegiumParser {
  Terminal<std::string_view> Word{"Word", "word"_kw};
  Rule<ChoiceNode> Root{"Root", assign<&ChoiceNode::op>(Word | "-"_kw)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct StrictOrderedChoiceParser final : PegiumParser {
  Rule<StrictChildNode> Child{"Child",
                              assign<&StrictChildNode::name>("child"_kw)};
  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  Rule<StrictChoiceNode> Root{"Root",
                              assign<&StrictChoiceNode::value>(Child | Bool)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct AssignmentFeatureParser final : PegiumParser {
  Rule<DerivedLeafNode> Child{"Child",
                              assign<&DerivedLeafNode::name>("child"_kw)};
  Rule<AssignmentFeatureNode> Root{
      "Root",
      assign<&AssignmentFeatureNode::optName>("opt"_kw) + ":"_kw +
          assign<&AssignmentFeatureNode::refOne>("ref"_kw) + ":"_kw +
          assign<&AssignmentFeatureNode::child>(Child) + ":"_kw +
          many(append<&AssignmentFeatureNode::children>(Child), ","_kw)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct PointerChoiceParser final : PegiumParser {
  Rule<DerivedLeafNode> Child{"Child",
                              assign<&DerivedLeafNode::name>("child"_kw)};
  Rule<PointerChoiceNode> Root{
      "Root", assign<&PointerChoiceNode::child>(Child | Child)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct BoolEnableParser final : PegiumParser {
  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  Rule<BoolEnableNode> Root{"Root", enable_if<&BoolEnableNode::enabled>(Bool)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct VariantSourceParser final : PegiumParser {
  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  DataTypeRule<std::string> Word{"Word", "ab"_kw};
  Rule<VariantSourceNode> Root{"Root",
                               assign<&VariantSourceNode::value>(Bool | Word)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct CharAnyParser final : PegiumParser {
  Rule<CharAnyNode> Root{"Root", assign<&CharAnyNode::value>(d | dot)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct LiteralChoiceParser final : PegiumParser {
  Rule<LiteralChoiceNode> Root{
      "Root", assign<&LiteralChoiceNode::value>("z"_kw | "y"_kw)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct EnumChoiceParser final : PegiumParser {
  Terminal<AssignMode> ModeA{"ModeA", "A"_kw};
  Terminal<AssignMode> ModeB{"ModeB", "B"_kw};
  Rule<EnumChoiceNode> Root{
      "Root", assign<&EnumChoiceNode::mode>(ModeA | ModeB) + ":"_kw +
                  assign<&EnumChoiceNode::maybeMode>(ModeA | ModeB) + ":"_kw +
                  assign<&EnumChoiceNode::variantMode>(ModeA | ModeB)};

  EnumChoiceParser() {
    ModeA.setValueConverter([](std::string_view) noexcept {
      return opt::conversion_value<AssignMode>(AssignMode::Alpha);
    });
    ModeB.setValueConverter([](std::string_view) noexcept {
      return opt::conversion_value<AssignMode>(AssignMode::Beta);
    });
  }

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct BoolAssignParser final : PegiumParser {
  Rule<BoolEnableNode> Root{"Root", assign<&BoolEnableNode::enabled>("!"_kw)};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct DirectValueParser final : PegiumParser {
  Rule<DirectValueNode> Root{
      "Root", assign<&DirectValueNode::one>(LeafValueElement{}) + ":"_kw +
                  append<&DirectValueNode::many>(LeafValueElement{})};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

struct NullValueParser final : PegiumParser {
  Rule<NullAssignNode> Root{
      "Root", assign<&NullAssignNode::marker>(NullValueElement{})};

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
};

template <typename T> const T *ast_as(const ParseResult &result) {
  return pegium::ast_ptr_cast<T>(result.value);
}

template <typename RuleType>
auto parse_rule(const RuleType &rule, std::string_view text,
                const Skipper &skipper = SkipperBuilder().build(),
                const ParseOptions &options = {}) {
  
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  rule.parse(*document, skipper, {}, options);
  return std::move(document);
}

} // namespace

TEST(AssignmentTest, AssignAppendAndEnableIfPopulateAstNode) {
  AssignmentParser parser;

  auto document = parse_rule(parser.Root, "id:tag,tag!child");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<AssignmentNode>(result);
  ASSERT_TRUE(resultValue != nullptr);

  EXPECT_EQ(resultValue->id, "id");
  ASSERT_EQ(resultValue->tags.size(), 2u);
  EXPECT_EQ(resultValue->tags[0], "tag");
  EXPECT_EQ(resultValue->tags[1], "tag");
  EXPECT_TRUE(resultValue->enabled);

  ASSERT_TRUE(resultValue->child != nullptr);
  EXPECT_EQ(resultValue->child->name, "child");
  EXPECT_EQ(resultValue->child->getContainer(), resultValue);
}

TEST(AssignmentTest, OrderedChoiceAssignsMatchedLiteralText) {
  ChoiceParser parser;

  auto document = parse_rule(parser.Root, "+");
  auto &plus = document->parseResult;
  ASSERT_TRUE(plus.value);
  auto plusValue = ast_as<ChoiceNode>(plus);
  ASSERT_TRUE(plusValue != nullptr);
  EXPECT_EQ(plusValue->op, "+");

  document = parse_rule(parser.Root, "-");
  auto &minus = document->parseResult;
  ASSERT_TRUE(minus.value);
  auto minusValue = ast_as<ChoiceNode>(minus);
  ASSERT_TRUE(minusValue != nullptr);
  EXPECT_EQ(minusValue->op, "-");
}

TEST(AssignmentTest, OrderedChoiceAssignsStringViewToString) {
  StringViewChoiceParser parser;

  auto document = parse_rule(parser.Root, "word");
  auto &word = document->parseResult;
  ASSERT_TRUE(word.value);
  auto wordValue = ast_as<ChoiceNode>(word);
  ASSERT_TRUE(wordValue != nullptr);
  EXPECT_EQ(wordValue->op, "word");

  document = parse_rule(parser.Root, "-");
  auto &minus = document->parseResult;
  ASSERT_TRUE(minus.value);
  auto minusValue = ast_as<ChoiceNode>(minus);
  ASSERT_TRUE(minusValue != nullptr);
  EXPECT_EQ(minusValue->op, "-");
}

TEST(AssignmentTest, OrderedChoiceStrictModeDoesNotConvertAstPointerToBool) {
  StrictOrderedChoiceParser parser;

  auto document = parse_rule(parser.Root, "child");
  auto &child = document->parseResult;
  ASSERT_TRUE(child.value);
  auto childValue = ast_as<StrictChoiceNode>(child);
  ASSERT_TRUE(childValue != nullptr);
  EXPECT_TRUE(std::holds_alternative<pegium::AstNode::pointer<StrictChildNode>>(
      childValue->value));
  EXPECT_FALSE(std::holds_alternative<bool>(childValue->value));

  document = parse_rule(parser.Root, "true");
  auto &yes = document->parseResult;
  ASSERT_TRUE(yes.value);
  auto yesValue = ast_as<StrictChoiceNode>(yes);
  ASSERT_TRUE(yesValue != nullptr);
  EXPECT_TRUE(std::holds_alternative<bool>(yesValue->value));
  EXPECT_TRUE(std::get<bool>(yesValue->value));
}

TEST(AssignmentTest, OptionalReferenceAndPointerAssignmentsAreHandled) {
  AssignmentFeatureParser parser;

  auto document = parse_rule(parser.Root, "opt:ref:child:child,child");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<AssignmentFeatureNode>(result);
  ASSERT_TRUE(resultValue != nullptr);

  ASSERT_TRUE(resultValue->optName.has_value());
  EXPECT_EQ(*resultValue->optName, "opt");
  EXPECT_EQ(resultValue->refOne.getRefText(), "ref");
  ASSERT_TRUE(resultValue->refOne.hasRefNode());
  EXPECT_EQ(resultValue->refOne.getRefNode().getText(), "ref");
  EXPECT_EQ(resultValue->refOne.getRefNode().getBegin(), 4u);
  EXPECT_EQ(resultValue->refOne.getRefNode().getEnd(), 7u);

  ASSERT_TRUE(resultValue->child != nullptr);
  EXPECT_EQ(resultValue->child->name, "child");
  EXPECT_EQ(resultValue->child->getContainer(), resultValue);

  ASSERT_EQ(resultValue->children.size(), 2u);
  ASSERT_TRUE(resultValue->children[0] != nullptr);
  ASSERT_TRUE(resultValue->children[1] != nullptr);
  EXPECT_EQ(resultValue->children[0]->getContainer(), resultValue);
  EXPECT_EQ(resultValue->children[1]->getContainer(), resultValue);
}

TEST(AssignmentTest, ReferenceAssignmentsCaptureSourceCstNode) {
  AssignmentFeatureParser parser;

  auto document = parse_rule(parser.Root, "opt:ref:child:child");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<AssignmentFeatureNode>(result);
  ASSERT_TRUE(resultValue != nullptr);

  EXPECT_EQ(resultValue->refOne.getRefText(), "ref");
  ASSERT_TRUE(resultValue->refOne.hasRefNode());

  const auto &refNode = resultValue->refOne.getRefNode();
  EXPECT_EQ(refNode.getText(), "ref");
  EXPECT_EQ(refNode.getBegin(), 4u);
  EXPECT_EQ(refNode.getEnd(), 7u);
}

TEST(AssignmentTest, OrderedChoiceParserRuleValueUsesAstPointerPath) {
  PointerChoiceParser parser;

  auto document = parse_rule(parser.Root, "child");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<PointerChoiceNode>(result);
  ASSERT_TRUE(resultValue != nullptr);
  ASSERT_TRUE(resultValue->child != nullptr);
  EXPECT_EQ(resultValue->child->name, "child");
  EXPECT_EQ(resultValue->child->getContainer(), resultValue);
}

TEST(AssignmentTest, EnableIfWithBoolElementAssignsParsedBooleanValue) {
  BoolEnableParser parser;

  auto document = parse_rule(parser.Root, "true");
  auto &yes = document->parseResult;
  ASSERT_TRUE(yes.value);
  auto yesValue = ast_as<BoolEnableNode>(yes);
  ASSERT_TRUE(yesValue != nullptr);
  EXPECT_TRUE(yesValue->enabled);

  document = parse_rule(parser.Root, "false");
  auto &no = document->parseResult;
  ASSERT_TRUE(no.value);
  auto noValue = ast_as<BoolEnableNode>(no);
  ASSERT_TRUE(noValue != nullptr);
  EXPECT_FALSE(noValue->enabled);
}

TEST(AssignmentTest, OrderedChoiceSupportsTerminalAndDataTypeKinds) {
  VariantSourceParser parser;

  {
    auto document = parse_rule(parser.Root, "true");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<VariantSourceNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_TRUE(std::holds_alternative<bool>(resultValue->value));
    EXPECT_TRUE(std::get<bool>(resultValue->value));
  }

  {
    auto document = parse_rule(parser.Root, "ab");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<VariantSourceNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_TRUE(std::holds_alternative<std::string>(resultValue->value));
    EXPECT_EQ(std::get<std::string>(resultValue->value), "ab");
  }
}

TEST(AssignmentTest, OrderedChoiceSupportsLiteralKindWithStringFallback) {
  LiteralChoiceParser parser;

  auto document = parse_rule(parser.Root, "z");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<LiteralChoiceNode>(result);
  ASSERT_TRUE(resultValue != nullptr);
  EXPECT_EQ(resultValue->value, "z");
}

TEST(AssignmentTest, OrderedChoiceSupportsCharacterRangeAndAnyCharacterKinds) {
  CharAnyParser parser;

  {
    auto document = parse_rule(parser.Root, "7");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<CharAnyNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_EQ(resultValue->value, "7");
  }

  {
    auto document = parse_rule(parser.Root, ":");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<CharAnyNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_EQ(resultValue->value, ":");
  }
}

TEST(AssignmentTest, OrderedChoiceConvertsUnderlyingRuleValueBackToEnum) {
  EnumChoiceParser parser;

  {
    auto document = parse_rule(parser.Root, "A:B:A");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<EnumChoiceNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_EQ(resultValue->mode, AssignMode::Alpha);
    ASSERT_TRUE(resultValue->maybeMode.has_value());
    EXPECT_EQ(*resultValue->maybeMode, AssignMode::Beta);
    ASSERT_TRUE(std::holds_alternative<AssignMode>(resultValue->variantMode));
    EXPECT_EQ(std::get<AssignMode>(resultValue->variantMode),
              AssignMode::Alpha);
  }

  {
    auto document = parse_rule(parser.Root, "B:A:B");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<EnumChoiceNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_EQ(resultValue->mode, AssignMode::Beta);
    ASSERT_TRUE(resultValue->maybeMode.has_value());
    EXPECT_EQ(*resultValue->maybeMode, AssignMode::Alpha);
    ASSERT_TRUE(std::holds_alternative<AssignMode>(resultValue->variantMode));
    EXPECT_EQ(std::get<AssignMode>(resultValue->variantMode), AssignMode::Beta);
  }
}

TEST(AssignmentTest, ParseRuleForNonOrderedOverridesGrammarElement) {
  auto expression = assign<&AssignmentNode::id>("id"_kw);
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("id");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};

    auto ok = parse(expression, ctx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.cursor() - input.begin(), 2);

    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(expression));
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("xx");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    auto ko = parse(expression, ctx);
    EXPECT_FALSE(ko);

    auto root = builder.getRootCstNode();
    EXPECT_EQ(root->begin(), root->end());
  }
}

TEST(AssignmentTest, ParseRuleForOrderedChoiceFailureDoesNotRollbackLocally) {
  auto expression = assign<&ChoiceNode::op>("+"_kw | "-"_kw);
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("+");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};

    auto ok = parse(expression, ctx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.cursor() - input.begin(), 1);

    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(expression));
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("*");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto ko = parse(expression, ctx);
    EXPECT_FALSE(ko);
    EXPECT_EQ(ctx.cursor(), input.begin());

    auto root = builder.getRootCstNode();
    EXPECT_NE(root->begin(), root->end());
  }
}

TEST(AssignmentTest, AssignOnBoolWithNonBoolElementSetsTrue) {
  BoolAssignParser parser;

  auto document = parse_rule(parser.Root, "!");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<BoolEnableNode>(result);
  ASSERT_TRUE(resultValue != nullptr);
  EXPECT_TRUE(resultValue->enabled);
}

TEST(AssignmentTest, AssignmentMetadataAndOperatorsAreExposed) {
  auto assignExpr = assign<&AssignmentNode::id>("id"_kw);
  auto appendExpr = append<&AssignmentNode::tags>("tag"_kw);
  auto enableExpr = enable_if<&AssignmentNode::enabled>("!"_kw);

  EXPECT_EQ(assignExpr.getKind(), pegium::grammar::ElementKind::Assignment);
  EXPECT_EQ(assignExpr.getOperator(),
            pegium::grammar::AssignmentOperator::Assign);
  EXPECT_EQ(appendExpr.getOperator(),
            pegium::grammar::AssignmentOperator::Append);
  EXPECT_EQ(enableExpr.getOperator(),
            pegium::grammar::AssignmentOperator::EnableIf);

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

  auto document = parse_rule(parser.Root, "leaf:leaf");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<DirectValueNode>(result);
  ASSERT_TRUE(resultValue != nullptr);

  ASSERT_TRUE(resultValue->one != nullptr);
  EXPECT_EQ(resultValue->one->name, "leaf");

  ASSERT_EQ(resultValue->many.size(), 1u);
  ASSERT_TRUE(resultValue->many[0] != nullptr);
  EXPECT_EQ(resultValue->many[0]->name, "leaf");
}

TEST(AssignmentTest, NullptrValueAssignmentIsAccepted) {
  NullValueParser parser;

  auto document = parse_rule(parser.Root, "nil");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto resultValue = ast_as<NullAssignNode>(result);
  ASSERT_TRUE(resultValue != nullptr);
  EXPECT_EQ(resultValue->marker, nullptr);
}

TEST(AssignmentTest,
     OrderedChoiceExecuteThrowsWhenSelectedValueIsNotAssignable) {
  auto expression = assign<&ChoiceNode::op>("+"_kw | "-"_kw);
  ParserRule<StrictChildNode> childRule{
      "Child", assign<&StrictChildNode::name>("child"_kw)};

  auto builderHarness = pegium::test::makeCstBuilderHarness("child");
  auto &builder = builderHarness.builder;
  const char *begin = builder.input_begin();
  builder.enter();
  builder.leaf(begin, begin + 5, std::addressof(childRule), false);
  builder.exit(begin, begin + 5, std::addressof(expression));
  auto root = builder.getRootCstNode();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  ChoiceNode current;
  EXPECT_THROW(expression.execute(&current, *node), std::runtime_error);
}

TEST(AssignmentTest, OrderedChoiceExecuteThrowsWhenOnlyHiddenChildrenExist) {
  auto expression = assign<&LiteralChoiceNode::value>("z"_kw | "y"_kw);
  const auto hiddenToken = " "_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness(" ");
  auto &builder = builderHarness.builder;
  const char *begin = builder.input_begin();
  builder.enter();
  builder.leaf(begin, begin + 1, std::addressof(hiddenToken), true);
  builder.exit(begin, begin + 1, std::addressof(expression));
  auto root = builder.getRootCstNode();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  LiteralChoiceNode current;
  EXPECT_THROW(expression.execute(&current, *node), std::runtime_error);
}

TEST(AssignmentTest, OrderedChoiceExecuteSkipsHiddenChildren) {
  const auto zToken = "z"_kw;
  const auto yToken = "y"_kw;
  auto expression = assign<&LiteralChoiceNode::value>(zToken | yToken);
  const auto hiddenToken = " "_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("z");
  auto &builder = builderHarness.builder;
  const char *begin = builder.input_begin();
  builder.enter();
  builder.leaf(begin, begin, std::addressof(hiddenToken), true);
  builder.leaf(begin, begin + 1, std::addressof(zToken), false);
  builder.exit(begin, begin + 1, std::addressof(expression));
  auto root = builder.getRootCstNode();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  LiteralChoiceNode current;
  expression.execute(&current, *node);
  EXPECT_EQ(current.value, "z");
}

TEST(AssignmentTest, OrderedChoiceExecuteRejectsTerminalEquivalentAddressMiss) {
  const auto zToken = "z"_kw;
  const auto yToken = "y"_kw;
  auto expression = assign<&LiteralChoiceNode::value>(zToken | yToken);
  const auto otherZToken = "z"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("z");
  auto &builder = builderHarness.builder;
  const char *begin = builder.input_begin();
  builder.enter();
  builder.leaf(begin, begin + 1, std::addressof(otherZToken), false);
  builder.exit(begin, begin + 1, std::addressof(expression));
  auto root = builder.getRootCstNode();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  LiteralChoiceNode current;
  EXPECT_THROW(expression.execute(&current, *node), std::runtime_error);
}
