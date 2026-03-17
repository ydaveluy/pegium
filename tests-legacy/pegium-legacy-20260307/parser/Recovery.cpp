#include <gtest/gtest.h>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>

#include <algorithm>
#include <string>

using namespace pegium::parser;

namespace {

struct RecoveryNode : pegium::AstNode {
  string token;
};

template <typename T>
struct ValueNode : pegium::AstNode {
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

  template <typename ParseFn>
  ParsedDocument(std::string_view text, ParseFn &&parseFn)
      : parseResult(document.parseResult), cst(parseResult.cst),
        value(parseResult.value),
        parseDiagnostics(parseResult.parseDiagnostics),
        parsedLength(parseResult.parsedLength), fullMatch(parseResult.fullMatch) {
    document.setText(std::string{text});
    parseFn(document);
  }
};

template <typename T>
ParsedDocument parseDataType(const DataTypeRule<T> &rule, std::string_view text,
                             const Skipper &skipper,
                             const ParseOptions &options = {}) {
  ParserRule<ValueNode<T>> root{"Root", assign<&ValueNode<T>::value>(rule)};
  return ParsedDocument{text,
                        [&](pegium::workspace::Document &document) {
                          root.parse(document, skipper, {}, options);
                        }};
}

template <typename T>
ParsedDocument parseTerminal(const TerminalRule<T> &rule, std::string_view text,
                             const Skipper &skipper,
                             const ParseOptions &options = {}) {
  ParserRule<ValueNode<T>> root{"Root", assign<&ValueNode<T>::value>(rule)};
  return ParsedDocument{text,
                        [&](pegium::workspace::Document &document) {
                          root.parse(document, skipper, {}, options);
                        }};
}

template <typename RuleType>
ParsedDocument parseRule(const RuleType &rule, std::string_view text,
                         const Skipper &skipper,
                         const ParseOptions &options = {}) {
  return ParsedDocument{text,
                        [&](pegium::workspace::Document &document) {
                          rule.parse(document, skipper, {}, options);
                        }};
}

} // namespace

TEST(RecoveryTest, DeleteBudgetIsConfigurable) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const std::string input = "xxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  const auto defaultResult = parseDataType(rule, input, skipper);
  EXPECT_FALSE(defaultResult.value);
  EXPECT_TRUE(defaultResult.parseDiagnostics.empty());

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 16;
  const auto tunedResult = parseDataType(rule, input, skipper, options);
  EXPECT_TRUE(tunedResult.value);
  EXPECT_FALSE(tunedResult.parseDiagnostics.empty());
}

TEST(RecoveryTest, DiagnosticsTrackDeleteAndInsertEdits) {
  const auto skipper = SkipperBuilder().build();

  {
    DataTypeRule<std::string> rule{"Rule", "service"_kw};
    const std::string input = "oopsservice";
    const auto result = parseDataType(rule, input, skipper);

    ASSERT_TRUE(result.value);
    ASSERT_FALSE(result.parseDiagnostics.empty());
    EXPECT_TRUE(std::ranges::all_of(result.parseDiagnostics,
                                    [](const ParseDiagnostic &d) {
                                      return d.kind == ParseDiagnosticKind::Deleted;
                                    }));
  }

  {
    DataTypeRule<std::string> rule{"Rule", "service"_kw + "{"_kw + "}"_kw};
    const std::string input = "service{";
    const auto result = parseDataType(rule, input, skipper);

    ASSERT_TRUE(result.value);
    ASSERT_FALSE(result.parseDiagnostics.empty());
    EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                    [](const ParseDiagnostic &d) {
                                      return d.kind == ParseDiagnosticKind::Inserted;
                                    }));
  }

}

TEST(RecoveryTest, GenericLiteralFuzzyRecoveryRepairsSingleEditOsaShapes) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const auto skipper = SkipperBuilder().build();

  {
    const std::string input = "servixe";
    const auto result = parseDataType(rule, input, skipper);
    EXPECT_TRUE(result.value);
    EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                    [](const ParseDiagnostic &diagnostic) {
                                      return diagnostic.kind ==
                                             ParseDiagnosticKind::Replaced;
                                    }));
  }

  {
    const std::string input = "serivce";
    const auto result = parseDataType(rule, input, skipper);
    EXPECT_TRUE(result.value);
    EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                    [](const ParseDiagnostic &diagnostic) {
                                      return diagnostic.kind ==
                                             ParseDiagnosticKind::Replaced;
                                    }));
  }

  {
    const std::string input = "sxrivxe";
    const auto result = parseDataType(rule, input, skipper);
    EXPECT_FALSE(result.value);
  }
}

TEST(RecoveryTest, AnyCharacterRecoveryDoesNotInventMissingCharacter) {
  DataTypeRule<std::string_view> rule{"Rule", dot};
  const auto result = parseDataType(rule, "", SkipperBuilder().build());

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(std::ranges::none_of(result.parseDiagnostics,
                                   [](const ParseDiagnostic &d) {
                                     return d.kind ==
                                            ParseDiagnosticKind::Inserted;
                                   }));
}

TEST(RecoveryTest, AndPredicateRecoveryDoesNotUseEdits) {
  DataTypeRule<std::string> rule{"Rule", &"a"_kw + "a"_kw};
  const std::string input = "xa";
  const auto result = parseDataType(rule, input, SkipperBuilder().build());

  EXPECT_FALSE(result.value);
}

TEST(RecoveryTest, RecoveryCanBeDisabledThroughParseOptions) {
  const std::string input = "oopsservice";
  const auto skipper = SkipperBuilder().build();

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 16;
  options.recoveryEnabled = false;

  {
    DataTypeRule<std::string> rule{"DataRule", "service"_kw};
    const auto result = parseDataType(rule, input, skipper, options);
    EXPECT_FALSE(result.value);
    EXPECT_TRUE(result.parseDiagnostics.empty());
  }

  {
    TerminalRule<std::string_view> rule{"TerminalRule", "service"_kw};
    const auto result = parseTerminal(rule, input, skipper, options);
    EXPECT_FALSE(result.value);
    EXPECT_TRUE(result.parseDiagnostics.empty());
  }

  {
    ParserRule<RecoveryNode> rule{"ParserRule",
                                  assign<&RecoveryNode::token>("service"_kw)};
    const auto result = parseRule(rule, input, skipper, options);
    EXPECT_FALSE(result.value);
    EXPECT_TRUE(result.parseDiagnostics.empty());
  }
}

TEST(RecoveryTest,
     ConsecutiveDeleteRecoveryAcrossTerminalBoundaryIsNotGuaranteedGenerically) {
  DataTypeRule<std::string> rule{"Rule", "aaa"_kw + "{"_kw};
  const std::string input = "aaaXXX{";
  const auto skipper = SkipperBuilder().build();

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 8;

  const auto result = parseDataType(rule, input, skipper, options);
  EXPECT_FALSE(result.value);
}

TEST(RecoveryTest, DeleteBudgetOptionIsAppliedByParserRule) {
  const std::string input = "xxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  ParserRule<RecoveryNode> rule{"ParserRule",
                                assign<&RecoveryNode::token>("service"_kw)};
  const auto defaultResult = parseRule(rule, input, skipper);
  EXPECT_FALSE(defaultResult.value);
  EXPECT_TRUE(defaultResult.parseDiagnostics.empty());

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 16;
  const auto tunedResult = parseRule(rule, input, skipper, options);
  EXPECT_TRUE(tunedResult.value);
  EXPECT_FALSE(tunedResult.parseDiagnostics.empty());
  auto *typed = pegium::ast_ptr_cast<RecoveryNode>(tunedResult.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->token, "service");
}
