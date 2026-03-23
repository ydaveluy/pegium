#include <gtest/gtest.h>
#include <pegium/ParseJsonTestSupport.hpp>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <array>
#include <cctype>
#include <ranges>
#include <vector>

using namespace pegium::parser;

namespace {

template <typename T>
struct DataValueNode : pegium::AstNode {
  T value{};
};

struct ParsedResult {
  pegium::parser::ParseResult result;
  std::unique_ptr<pegium::RootCstNode> &cst;
  std::unique_ptr<pegium::AstNode> &value;
  std::vector<pegium::parser::ParseDiagnostic> &parseDiagnostics;
  pegium::TextOffset &parsedLength;
  bool &fullMatch;

  explicit ParsedResult(pegium::parser::ParseResult parseResult)
      : result(std::move(parseResult)), cst(result.cst), value(result.value),
        parseDiagnostics(result.parseDiagnostics),
        parsedLength(result.parsedLength), fullMatch(result.fullMatch) {}
};

template <typename T>
ParsedResult
parseDataTypeRule(const DataTypeRule<T> &rule, std::string_view text,
                  const Skipper &skipper,
                  const ParseOptions &options = {}) {
  ParserRule<DataValueNode<T>> root{"Root", assign<&DataValueNode<T>::value>(rule)};
  return ParsedResult{
      pegium::test::parse_rule_result(root, text, skipper, options)};
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
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
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

TEST(DataTypeRuleTest, NonStringRuleWithoutConverterProducesDiagnostic) {
  DataTypeRule<int> rule{"Rule", "123"_kw};

  auto result = parseDataTypeRule(rule, "123", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_NE(result.parseDiagnostics.front().message.find("ValueConvert"),
            std::string::npos);

  auto *typed = pegium::ast_ptr_cast<DataValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 0);
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

TEST(DataTypeRuleTest, ConverterCanSetValue) {
  DataTypeRule<int> rule{
      "Rule", "42"_kw,
      opt::with_converter(
          [](std::string_view text) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_value<int>(static_cast<int>(text.size()) + 5);
          })};

  auto result = parseDataTypeRule(rule, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.parseDiagnostics.empty());

  auto *typed = pegium::ast_ptr_cast<DataValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 7);
}

TEST(DataTypeRuleTest,
     ConverterFailureProducesConversionDiagnosticAndFallbackValue) {
  DataTypeRule<int> rule{
      "Rule", "42"_kw,
      opt::with_converter(
          [](std::string_view) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_error<int>("bad datatype");
          })};

  auto result = parseDataTypeRule(rule, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics.front().message, "bad datatype");

  auto *typed = pegium::ast_ptr_cast<DataValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 0);
}

TEST(DataTypeRuleTest, RecoveredConverterFailureStillProducesDiagnostic) {
  DataTypeRule<int> rule{
      "Rule", "x"_kw + "y"_kw,
      opt::with_converter([](std::string_view sv) noexcept
                              -> opt::ConversionResult<int> {
        if (sv != "xy") {
          return opt::conversion_error<int>("bad recovered datatype");
        }
        return opt::conversion_value<int>(2);
      })};

  auto builderHarness = pegium::test::makeCstBuilderHarness("xz");
  auto &builder = builderHarness.builder;
  builder.leaf(0, static_cast<pegium::TextOffset>(builder.getText().size()),
               std::addressof(rule), false, true);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  const auto node = *it;
  ASSERT_TRUE(node.isRecovered());

  std::vector<ParseDiagnostic> diagnostics;
  const ValueBuildContext context{.diagnostics = &diagnostics};
  EXPECT_EQ(rule.getRawValue(node, context), 0);
  EXPECT_TRUE(std::ranges::any_of(
      diagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::ConversionError &&
               diagnostic.message == "bad recovered datatype";
      }));
}

TEST(DataTypeRuleTest, ArithmeticChainCstRangesAreCorrect) {
  TerminalRule<> ws{"WS", some(s)};
  DataTypeRule<std::string> number{"Number", some(d)};
  ParserRule<pegium::AstNode> expression{
      "Expression", number + many("+"_kw + number)};

  constexpr std::string_view input = "1 + 2 + 3 +4";
  const auto result =
      pegium::test::Parse(expression, input, SkipperBuilder().ignore(ws).build());
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
