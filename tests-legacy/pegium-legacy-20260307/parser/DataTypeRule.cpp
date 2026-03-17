#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>
#include <array>
#include <cctype>
#include <memory>
#include <ostream>
#include <variant>
#include <vector>

using namespace pegium::parser;

namespace {

struct DummyElement final : pegium::grammar::AbstractElement {
  explicit DummyElement(ElementKind kind) : kind(kind) {}

  constexpr ElementKind getKind() const noexcept override { return kind; }
  void print(std::ostream &os) const override { os << "dummy"; }

  ElementKind kind;
};

template <typename T>
struct DataValueNode : pegium::AstNode {
  T value{};
};

struct ParsedDocument {
  pegium::workspace::Document document;
  pegium::parser::ParseResult &parseResult;
  std::unique_ptr<pegium::RootCstNode> &cst;
  std::unique_ptr<pegium::AstNode> &value;
  std::vector<pegium::parser::ParseDiagnostic> &parseDiagnostics;
  pegium::TextOffset &parsedLength;
  bool &fullMatch;

  template <typename RuleType>
  ParsedDocument(const RuleType &rule, std::string_view text,
                 const Skipper &skipper,
                 const ParseOptions &options = {})
      : parseResult(document.parseResult), cst(parseResult.cst),
        value(parseResult.value),
        parseDiagnostics(parseResult.parseDiagnostics),
        parsedLength(parseResult.parsedLength), fullMatch(parseResult.fullMatch) {
    document.setText(std::string{text});
    rule.parse(document, skipper, {}, options);
  }
};

template <typename T>
ParsedDocument
parseDataTypeRule(const DataTypeRule<T> &rule, std::string_view text,
                  const Skipper &skipper,
                  const ParseOptions &options = {}) {
  ParserRule<DataValueNode<T>> root{"Root", assign<&DataValueNode<T>::value>(rule)};
  return ParsedDocument{root, text, skipper, options};
}

} // namespace

TEST(DataTypeRuleTest, ParseRequiresFullConsumption) {
  DataTypeRule<std::string> rule{"Rule", ":"_kw};

  {
    auto result = parseDataTypeRule(rule, ":", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed = pegium::ast_ptr_cast<DataValueNode<std::string>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, ":");
  }

  {
    auto result = parseDataTypeRule(rule, ":abc", SkipperBuilder().build());
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
    auto *typed =
        pegium::ast_ptr_cast<DataValueNode<std::string>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, ":");
  }
}

TEST(DataTypeRuleTest, StringRuleConcatenatesVisibleTokens) {
  TerminalRule<> ws{"WS", some(s)};
  DataTypeRule<std::string> rule{"Rule", "a"_kw + "b"_kw};

  auto result = parseDataTypeRule(rule, "a   b", SkipperBuilder().ignore(ws).build());
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<DataValueNode<std::string>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, "ab");
}

TEST(DataTypeRuleTest, NonStringRuleRequiresValueConverter) {
  DataTypeRule<int> rule{"Rule", "123"_kw};
  EXPECT_THROW((void)parseDataTypeRule(rule, "123", SkipperBuilder().build()),
               std::logic_error);
}

TEST(DataTypeRuleTest, ParseFailureRewindsCursor) {
  DataTypeRule<std::string> rule{"Rule", "abc"_kw};
  auto result = parseDataTypeRule(rule, "zzz", SkipperBuilder().build());
  EXPECT_FALSE(result.value);
}

TEST(DataTypeRuleTest, StringRuleHandlesCharacterRangeAndAnyCharacterKinds) {
  DataTypeRule<std::string> digit{"Digit", d};
  DataTypeRule<std::string> any{"Any", dot};

  {
    auto result = parseDataTypeRule(digit, "7", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    auto *typed = pegium::ast_ptr_cast<DataValueNode<std::string>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "7");
  }

  {
    auto result = parseDataTypeRule(any, "X", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    auto *typed = pegium::ast_ptr_cast<DataValueNode<std::string>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "X");
  }
}

TEST(DataTypeRuleTest, StringRuleThrowsForUnsupportedGrammarChildKind) {
  DataTypeRule<std::string> rule{"Rule", "ab"_kw};
  DummyElement unsupported{pegium::grammar::ElementKind::Group};

  pegium::workspace::Document document;
  document.setText("ab");
  auto builderHarness = pegium::test::makeCstBuilderHarness(document);
  auto &builder = builderHarness.builder;
  const char *begin = builder.input_begin();
  builder.enter();
  builder.leaf(begin, begin + 2, &unsupported, false);
  builder.exit(begin, begin + 2, std::addressof(rule));
  auto root = builder.getRootCstNode();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  EXPECT_THROW((void)rule.getValue(*node), std::logic_error);
}

TEST(DataTypeRuleTest, GetValueAndParseReturnRuleValueVariant) {
  DataTypeRule<std::string> rule{"Rule", "hello"_kw};

  auto skipper = SkipperBuilder().build();
  pegium::workspace::Document document;
  document.setText("hello");
  auto builderHarness = pegium::test::makeCstBuilderHarness(document);
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, skipper};
  ASSERT_TRUE(parse(rule, ctx));
  auto root = builder.getRootCstNode();
  auto node = root->begin();
  ASSERT_NE(node, root->end());

  auto value = rule.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::string>(value));
  EXPECT_EQ(std::get<std::string>(value), "hello");
  EXPECT_FALSE(rule.getTypeName().empty());
}

TEST(DataTypeRuleTest, ParseRuleAddsNodeOnSuccessAndKeepsLocalFailureState) {
  DataTypeRule<std::string> rule{"Rule", "ab"_kw};
  auto context = SkipperBuilder().build();

  {
    pegium::workspace::Document document;
    document.setText("ab");
    auto builderHarness = pegium::test::makeCstBuilderHarness(document);
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, context};

    auto ok = parse(rule, ctx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.cursor() - input.begin(), 2);

    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(rule));
  }

  {
    pegium::workspace::Document document;
    document.setText("zz");
    auto builderHarness = pegium::test::makeCstBuilderHarness(document);
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, context};

    auto ko = parse(rule, ctx);
    EXPECT_FALSE(ko);
    EXPECT_EQ(ctx.cursor(), input.begin());

    auto root = builder.getRootCstNode();
    EXPECT_NE(root->begin(), root->end());
  }
}

TEST(DataTypeRuleTest, ConstructorOptionsSetLocalSkipperAndConverter) {
  TerminalRule<> ws{"WS", some(s)};

  DataTypeRule<std::size_t> rule{
      "Rule",
      "a"_kw + "b"_kw,
      opt::with_skipper(SkipperBuilder().ignore(ws).build()),
      opt::with_converter([](const pegium::CstNodeView &node) noexcept
                              -> opt::ConversionResult<std::size_t> {
        return opt::conversion_value<std::size_t>(node.getText().size());
      })};

  auto result = parseDataTypeRule(rule, "a   b", SkipperBuilder().build());
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<DataValueNode<std::size_t>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 5u);
}

TEST(DataTypeRuleTest, ArithmeticChainCstRangesAreCorrect) {
  TerminalRule<> ws{"WS", some(s)};
  DataTypeRule<std::string> number{"Number", some(d)};
  ParserRule<pegium::AstNode> expression{
      "Expression", number + many("+"_kw + number)};

  constexpr std::string_view input = "1 + 2 + 3 +4";
  pegium::workspace::Document document;
  document.setText(std::string{input});
  expression.parse(document, SkipperBuilder().ignore(ws).build());
  const auto &result = document.parseResult;
  ASSERT_TRUE(result.value != nullptr);
  ASSERT_TRUE(result.cst != nullptr);

  std::vector<pegium::CstNodeView> topLevelNodes;
  for (const auto node : *result.cst) {
    topLevelNodes.push_back(node);
  }
  ASSERT_EQ(topLevelNodes.size(), 1u);
  EXPECT_EQ(topLevelNodes.front().getBegin(), 0u);
  EXPECT_EQ(topLevelNodes.front().getEnd(), static_cast<pegium::TextOffset>(input.size()));

  std::vector<pegium::CstNodeView> allNodes;
  auto collect_nodes = [&allNodes](auto &&self,
                                   const pegium::CstNodeView &node) -> void {
    allNodes.push_back(node);
    for (const auto child : node) {
      self(self, child);
    }
  };
  for (const auto node : topLevelNodes) {
    collect_nodes(collect_nodes, node);
  }

  ASSERT_FALSE(allNodes.empty());

  struct TokenRange {
    std::string_view text;
    pegium::TextOffset begin;
    pegium::TextOffset end;
  };
  std::vector<TokenRange> visibleLeafRanges;
  std::vector<std::pair<pegium::TextOffset, pegium::TextOffset>> numberRanges;

  for (const auto node : allNodes) {
    const auto begin = node.getBegin();
    const auto end = node.getEnd();
    ASSERT_LE(begin, end);
    ASSERT_LE(end, static_cast<pegium::TextOffset>(input.size()));
    const auto text = node.getText();
    EXPECT_EQ(text, input.substr(begin, end - begin));

    if (!node.isHidden() &&
        node.getGrammarElement()->getKind() ==
            pegium::grammar::ElementKind::DataTypeRule &&
        text.size() == 1 &&
        std::isdigit(static_cast<unsigned char>(text.front())) != 0) {
      numberRanges.emplace_back(begin, end);
    }

    if (!node.isLeaf()) {
      auto firstIt = node.begin();
      ASSERT_NE(firstIt, node.end());
      const auto firstChild = *firstIt;
      auto lastChild = firstChild;
      for (const auto child : node) {
        lastChild = child;
      }
      EXPECT_EQ(begin, firstChild.getBegin());
      EXPECT_EQ(end, lastChild.getEnd());
      continue;
    }

    if (node.isHidden()) {
      continue;
    }
    visibleLeafRanges.push_back(TokenRange{node.getText(), begin, end});
  }

  const std::array<TokenRange, 7> expectedVisibleLeaves{{
      {"1", 0u, 1u},
      {"+", 2u, 3u},
      {"2", 4u, 5u},
      {"+", 6u, 7u},
      {"3", 8u, 9u},
      {"+", 10u, 11u},
      {"4", 11u, 12u},
  }};

  ASSERT_EQ(visibleLeafRanges.size(), expectedVisibleLeaves.size());
  for (std::size_t index = 0; index < expectedVisibleLeaves.size(); ++index) {
    EXPECT_EQ(visibleLeafRanges[index].text, expectedVisibleLeaves[index].text);
    EXPECT_EQ(visibleLeafRanges[index].begin,
              expectedVisibleLeaves[index].begin);
    EXPECT_EQ(visibleLeafRanges[index].end, expectedVisibleLeaves[index].end);
  }

  const std::array<std::pair<pegium::TextOffset, pegium::TextOffset>, 4>
      expectedNumberRanges{{
          {0u, 1u},
          {4u, 5u},
          {8u, 9u},
          {11u, 12u},
      }};

  ASSERT_EQ(numberRanges.size(), expectedNumberRanges.size());
  for (std::size_t index = 0; index < expectedNumberRanges.size(); ++index) {
    EXPECT_EQ(numberRanges[index], expectedNumberRanges[index]);
  }
}
