#include <cstdint>
#include <gtest/gtest.h>
#include <pegium/ParseJsonTestSupport.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/parser/PegiumParser.hpp>
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

struct ReferenceInfoNode : pegium::AstNode {
  reference<DerivedLeafNode> refOne;
  vector<reference<DerivedLeafNode>> refs;
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

struct ReferenceInfoParser final : PegiumParser {
  Rule<ReferenceInfoNode> Root{
      "Root",
      assign<&ReferenceInfoNode::refOne>("opt"_kw) + ":"_kw +
          many(append<&ReferenceInfoNode::refs>("ref"_kw), ","_kw)};

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
  pegium::test::parse_rule(rule, *document, skipper, options);
  return document;
}

} // namespace

TEST(AssignmentTest, AssignAppendAndEnableIfPopulateAstNode) {
  AssignmentParser parser;

  auto document = pegium::test::ExpectAst(
      parser.Root, "id:tag,tag!child",
      R"json({
  "$type": "AssignmentNode",
  "child": {
    "$type": "ChildNode",
    "name": "child"
  },
  "enabled": true,
  "id": "id",
  "tags": [
    "tag",
    "tag"
  ]
})json");
  auto resultValue = ast_as<AssignmentNode>(document->parseResult);
  ASSERT_TRUE(resultValue != nullptr);
  ASSERT_TRUE(resultValue->child != nullptr);
  EXPECT_EQ(resultValue->child->getContainer(), resultValue);
}

TEST(AssignmentTest, OrderedChoiceAssignsMatchedLiteralText) {
  ChoiceParser parser;

  pegium::test::ExpectAst(
      parser.Root, "+",
      R"json({
  "$type": "ChoiceNode",
  "op": "+"
})json");

  pegium::test::ExpectAst(
      parser.Root, "-",
      R"json({
  "$type": "ChoiceNode",
  "op": "-"
})json");
}

TEST(AssignmentTest, OptionalReferenceAndPointerAssignmentsAreHandled) {
  AssignmentFeatureParser parser;

  auto document = pegium::test::ExpectAst(
      parser.Root, "opt:ref:child:child,child",
      R"json({
  "$type": "AssignmentFeatureNode",
  "child": {
    "$type": "DerivedLeafNode",
    "name": "child"
  },
  "children": [
    {
      "$type": "DerivedLeafNode",
      "name": "child"
    },
    {
      "$type": "DerivedLeafNode",
      "name": "child"
    }
  ],
  "optName": "opt",
  "refOne": {
    "$refText": "ref"
  }
})json",
      {.includeReferenceErrors = false});
  auto resultValue = ast_as<AssignmentFeatureNode>(document->parseResult);
  ASSERT_TRUE(resultValue != nullptr);

  const auto refOneNode = resultValue->refOne.getRefNode();
  ASSERT_TRUE(refOneNode.has_value());
  EXPECT_EQ(refOneNode->getText(), "ref");
  EXPECT_EQ(refOneNode->getBegin(), 4u);
  EXPECT_EQ(refOneNode->getEnd(), 7u);

  ASSERT_TRUE(resultValue->child != nullptr);
  EXPECT_EQ(resultValue->child->getContainer(), resultValue);

  ASSERT_EQ(resultValue->children.size(), 2u);
  ASSERT_TRUE(resultValue->children[0] != nullptr);
  ASSERT_TRUE(resultValue->children[1] != nullptr);
  EXPECT_EQ(resultValue->children[0]->getContainer(), resultValue);
  EXPECT_EQ(resultValue->children[1]->getContainer(), resultValue);
}

TEST(AssignmentTest, ReferenceAssignmentsCaptureSourceCstNode) {
  AssignmentFeatureParser parser;

  auto document = pegium::test::ExpectAst(
      parser.Root, "opt:ref:child:child",
      R"json({
  "$type": "AssignmentFeatureNode",
  "child": {
    "$type": "DerivedLeafNode",
    "name": "child"
  },
  "children": [
    {
      "$type": "DerivedLeafNode",
      "name": "child"
    }
  ],
  "optName": "opt",
  "refOne": {
    "$refText": "ref"
  }
})json",
      {.includeReferenceErrors = false});
  auto resultValue = ast_as<AssignmentFeatureNode>(document->parseResult);
  ASSERT_TRUE(resultValue != nullptr);

  const auto refNode = resultValue->refOne.getRefNode();
  ASSERT_TRUE(refNode.has_value());
  EXPECT_EQ(refNode->getText(), "ref");
  EXPECT_EQ(refNode->getBegin(), 4u);
  EXPECT_EQ(refNode->getEnd(), 7u);
}

TEST(AssignmentTest, ReferenceAssignmentsPropagatePropertyAndIndexMetadata) {
  ReferenceInfoParser parser;

  auto document = pegium::test::ExpectAst(
      parser.Root, "opt:ref,ref",
      R"json({
  "$type": "ReferenceInfoNode",
  "refOne": {
    "$refText": "opt"
  },
  "refs": [
    {
      "$refText": "ref"
    },
    {
      "$refText": "ref"
    }
  ]
})json",
      {.includeReferenceErrors = false});
  auto resultValue = ast_as<ReferenceInfoNode>(document->parseResult);
  ASSERT_TRUE(resultValue != nullptr);

  ASSERT_EQ(document->references.size(), 3u);

  const auto *optionalRef = document->references[0].getConst();
  ASSERT_NE(optionalRef, nullptr);
  EXPECT_EQ(optionalRef->getProperty(), "refOne");
  EXPECT_FALSE(optionalRef->getPropertyIndex().has_value());

  const auto *firstRef = document->references[1].getConst();
  ASSERT_NE(firstRef, nullptr);
  EXPECT_EQ(firstRef->getProperty(), "refs");
  ASSERT_TRUE(firstRef->getPropertyIndex().has_value());
  EXPECT_EQ(*firstRef->getPropertyIndex(), 0u);

  const auto *secondRef = document->references[2].getConst();
  ASSERT_NE(secondRef, nullptr);
  EXPECT_EQ(secondRef->getProperty(), "refs");
  ASSERT_TRUE(secondRef->getPropertyIndex().has_value());
  EXPECT_EQ(*secondRef->getPropertyIndex(), 1u);
}

TEST(AssignmentTest, OrderedChoiceParserRuleValueUsesAstPointerPath) {
  PointerChoiceParser parser;

  auto document = pegium::test::ExpectAst(
      parser.Root, "child",
      R"json({
  "$type": "PointerChoiceNode",
  "child": {
    "$type": "DerivedLeafNode",
    "name": "child"
  }
})json");
  auto resultValue = ast_as<PointerChoiceNode>(document->parseResult);
  ASSERT_TRUE(resultValue != nullptr);
  ASSERT_TRUE(resultValue->child != nullptr);
  EXPECT_EQ(resultValue->child->getContainer(), resultValue);
}

TEST(AssignmentTest, EnableIfWithBoolElementAssignsParsedBooleanValue) {
  BoolEnableParser parser;

  pegium::test::ExpectAst(
      parser.Root, "true",
      R"json({
  "$type": "BoolEnableNode",
  "enabled": true
})json");
  pegium::test::ExpectAst(
      parser.Root, "false",
      R"json({
  "$type": "BoolEnableNode",
  "enabled": false
})json");
}

TEST(AssignmentTest, OrderedChoiceSupportsVariantBoolAndAstNodeValues) {
  StrictOrderedChoiceParser parser;

  {
    auto document = pegium::test::ExpectAst(
        parser.Root, "true",
        R"json({
  "$type": "StrictChoiceNode",
  "value": true
})json");
    auto resultValue = ast_as<StrictChoiceNode>(document->parseResult);
    ASSERT_TRUE(resultValue != nullptr);
    ASSERT_TRUE(std::holds_alternative<bool>(resultValue->value));
    EXPECT_TRUE(std::get<bool>(resultValue->value));
  }

  {
    auto document = pegium::test::ExpectAst(
        parser.Root, "child",
        R"json({
  "$type": "StrictChoiceNode",
  "value": {
    "$type": "StrictChildNode",
    "name": "child"
  }
})json");
    auto resultValue = ast_as<StrictChoiceNode>(document->parseResult);
    ASSERT_TRUE(resultValue != nullptr);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<StrictChildNode>>(
        resultValue->value));
    const auto &child =
        std::get<std::unique_ptr<StrictChildNode>>(resultValue->value);
    ASSERT_TRUE(child != nullptr);
    EXPECT_EQ(child->getContainer(), resultValue);
    EXPECT_EQ(child->getContainerPropertyName(), "value");
  }
}

TEST(AssignmentTest, OrderedChoiceSupportsTerminalAndDataTypeKinds) {
  VariantSourceParser parser;

  {
    auto document = pegium::test::ExpectAst(
        parser.Root, "true",
        R"json({
  "$type": "VariantSourceNode",
  "value": true
})json");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<VariantSourceNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_TRUE(std::holds_alternative<bool>(resultValue->value));
    EXPECT_TRUE(std::get<bool>(resultValue->value));
  }

  {
    auto document = pegium::test::ExpectAst(
        parser.Root, "ab",
        R"json({
  "$type": "VariantSourceNode",
  "value": "ab"
})json");
    auto &result = document->parseResult;
    ASSERT_TRUE(result.value);
    auto resultValue = ast_as<VariantSourceNode>(result);
    ASSERT_TRUE(resultValue != nullptr);
    EXPECT_TRUE(std::holds_alternative<std::string>(resultValue->value));
    EXPECT_EQ(std::get<std::string>(resultValue->value), "ab");
  }
}

TEST(AssignmentTest, OrderedChoiceConvertsUnderlyingRuleValueBackToEnum) {
  EnumChoiceParser parser;

  {
    auto document = pegium::test::ExpectAst(
        parser.Root, "A:B:A",
        R"json({
  "$type": "EnumChoiceNode",
  "maybeMode": 2,
  "mode": 1,
  "variantMode": 1
})json");
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
    auto document = pegium::test::ExpectAst(
        parser.Root, "B:A:B",
        R"json({
  "$type": "EnumChoiceNode",
  "maybeMode": 1,
  "mode": 2,
  "variantMode": 2
})json");
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

TEST(AssignmentTest, AssignOnBoolWithNonBoolElementSetsTrue) {
  BoolAssignParser parser;

  pegium::test::ExpectAst(
      parser.Root, "!",
      R"json({
  "$type": "BoolEnableNode",
  "enabled": true
})json");
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

  pegium::test::ExpectAst(
      parser.Root, "nil",
      R"json({
  "$type": "NullAssignNode",
  "marker": null
})json");
}
