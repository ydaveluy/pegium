#include <gtest/gtest.h>
#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

#include <algorithm>
#include <string>

using namespace pegium::parser;

namespace {

struct RecoveryNode : pegium::AstNode {
  string token;
};

struct RecoveryStatement : pegium::AstNode {};

struct RecoveryDefinition : RecoveryStatement {
  string name;
  int value = 0;
};

struct RecoveryExpression : pegium::AstNode {};

struct RecoveryNumberExpression : RecoveryExpression {
  int value = 0;
};

struct RecoveryReferenceExpression : RecoveryExpression {
  string name;
};

struct RecoveryFunctionCall : RecoveryExpression {
  string name;
  vector<pointer<RecoveryExpression>> args;
};

struct RecoveryDefinitionWithExpr : pegium::AstNode {
  string name;
  pointer<RecoveryExpression> expr;
};

struct RecoveryParameter : pegium::AstNode {
  string name;
};

struct RecoveryDefinitionWithOptionalArgs : pegium::AstNode {
  string name;
  vector<pointer<RecoveryParameter>> args;
  pointer<RecoveryExpression> expr;
};

struct RecoveryEvaluation : RecoveryStatement {
  string name;
};

struct RecoveryExpressionEvaluation : RecoveryStatement {
  pointer<RecoveryExpression> expression;
};

struct RecoveryBinaryExpression : RecoveryExpression {
  pointer<RecoveryExpression> left;
  string op;
  pointer<RecoveryExpression> right;
};

struct RecoveryModule : pegium::AstNode {
  string name;
  vector<pointer<pegium::AstNode>> statements;
};

template <typename T>
struct ValueNode : pegium::AstNode {
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
ParsedResult parseDataType(const DataTypeRule<T> &rule, std::string_view text,
                           const Skipper &skipper,
                           const ParseOptions &options = {}) {
  ParserRule<ValueNode<T>> root{"Root", assign<&ValueNode<T>::value>(rule)};
  return ParsedResult{
      pegium::test::parse_rule_result(root, text, skipper, options)};
}

template <typename T>
ParsedResult parseTerminal(const TerminalRule<T> &rule, std::string_view text,
                           const Skipper &skipper,
                           const ParseOptions &options = {}) {
  ParserRule<ValueNode<T>> root{"Root", assign<&ValueNode<T>::value>(rule)};
  return ParsedResult{
      pegium::test::parse_rule_result(root, text, skipper, options)};
}

template <typename RuleType>
ParsedResult parseRule(const RuleType &rule, std::string_view text,
                       const Skipper &skipper,
                       const ParseOptions &options = {}) {
  return ParsedResult{
      pegium::test::parse_rule_result(rule, text, skipper, options)};
}

} // namespace

TEST(RecoveryTest, WordLiteralReplacementRespectsConfidenceBeforeDeleteBudget) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const std::string input = "xxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  const auto defaultResult = parseDataType(rule, input, skipper);
  EXPECT_FALSE(defaultResult.value);
  ASSERT_FALSE(defaultResult.parseDiagnostics.empty());
  EXPECT_EQ(defaultResult.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 16;
  options.maxRecoveryEditsPerAttempt = 16;
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
                                      return d.kind == ParseDiagnosticKind::Deleted ||
                                             d.kind == ParseDiagnosticKind::Replaced;
                                    }));
    EXPECT_EQ(result.parseDiagnostics.front().offset, 0u);
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
    const auto inserted =
        std::ranges::find_if(result.parseDiagnostics, [](const ParseDiagnostic &d) {
          return d.kind == ParseDiagnosticKind::Inserted;
        });
    ASSERT_NE(inserted, result.parseDiagnostics.end());
    EXPECT_EQ(inserted->offset, 8u);
  }

}

TEST(RecoveryTest, IncompleteDiagnosticAtEofUsesParseOffset) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  ParseOptions options;
  options.recoveryEnabled = false;

  const auto result = parseDataType(rule, "", SkipperBuilder().build(), options);

  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Incomplete);
  EXPECT_EQ(result.parseDiagnostics.front().offset, 0u);
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
    EXPECT_TRUE(result.value);
    EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                    [](const ParseDiagnostic &diagnostic) {
                                      return diagnostic.kind ==
                                             ParseDiagnosticKind::Replaced;
                                    }));
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
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
  }

  {
    TerminalRule<std::string_view> rule{"TerminalRule", "service"_kw};
    const auto result = parseTerminal(rule, input, skipper, options);
    EXPECT_FALSE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
  }

  {
    ParserRule<RecoveryNode> rule{"ParserRule",
                                  assign<&RecoveryNode::token>("service"_kw)};
    const auto result = parseRule(rule, input, skipper, options);
    EXPECT_FALSE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
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

TEST(RecoveryTest, MissingRequiredTokenRecoveryStillBuildsRootAndReportsSyntax) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryDefinition> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinition::name>(id) + ":"_kw +
          assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<RecoveryEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryEvaluation::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module \n\ndef a:4;", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_FALSE(result.parseDiagnostics.empty());
  auto *parsedModule =
      dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.isSyntax();
                                  }));
}

TEST(RecoveryTest, WordBoundaryViolationCanInsertSyntheticGapForKeywordLiteral) {
  const auto skipper = SkipperBuilder().build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "modulebasicMath", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const ParseDiagnostic &diagnostic) {
                                     return diagnostic.kind ==
                                            ParseDiagnosticKind::Deleted;
                                   }));

  const auto inserted =
      std::ranges::find_if(result.parseDiagnostics, [](const ParseDiagnostic &d) {
        return d.kind == ParseDiagnosticKind::Inserted;
      });
  ASSERT_NE(inserted, result.parseDiagnostics.end());
  EXPECT_EQ(inserted->offset, 6u);
  EXPECT_EQ(inserted->element, nullptr);
  EXPECT_EQ(inserted->message, "Expecting separator");
  ASSERT_NE(result.cst, nullptr);
  const auto recovered = detail::first_recovered_node(*result.cst);
  EXPECT_FALSE(recovered.valid());

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest, MissingKeywordCodepointCanRecoverLiteralAndContinue) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "modle basicMath", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Replaced;
                                  }));

  const auto replaced =
      std::ranges::find_if(result.parseDiagnostics, [](const ParseDiagnostic &d) {
        return d.kind == ParseDiagnosticKind::Replaced;
      });
  ASSERT_NE(replaced, result.parseDiagnostics.end());
  EXPECT_EQ(replaced->offset, 0u);
  EXPECT_NE(replaced->element, nullptr);

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest, MissingKeywordSuffixCanRecoverLiteralAndContinue) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "Mod basicMath", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Replaced;
                                  }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest, ExtraKeywordCodepointCanRecoverLiteralAndContinue) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "modulee basicMath", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Replaced;
                                  }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest, TransposedKeywordCodepointsCanRecoverLiteralAndContinue) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "modlue basicMath", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Replaced;
                                  }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest,
     ParserRuleRejectsLowConfidenceReplacementBeforeDeleteBudget) {
  const std::string input = "xxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  ParserRule<RecoveryNode> rule{"ParserRule",
                                assign<&RecoveryNode::token>("service"_kw)};
  const auto defaultResult = parseRule(rule, input, skipper);
  EXPECT_FALSE(defaultResult.value);
  ASSERT_FALSE(defaultResult.parseDiagnostics.empty());
  EXPECT_EQ(defaultResult.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 16;
  options.maxRecoveryEditsPerAttempt = 16;
  const auto tunedResult = parseRule(rule, input, skipper, options);
  EXPECT_TRUE(tunedResult.value);
  EXPECT_FALSE(tunedResult.parseDiagnostics.empty());
  auto *typed = pegium::ast_ptr_cast<RecoveryNode>(tunedResult.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->token, "service");
}

TEST(RecoveryTest, MissingRequiredExpressionBeforeDelimiterLeavesHole) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryDefinitionWithExpr> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithExpr::name>(id) + ":"_kw +
          assign<&RecoveryDefinitionWithExpr::expr>(expression) + ";"_kw};

  const auto result = parseRule(definition, "def a :;", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  auto *parsedDefinition =
      dynamic_cast<RecoveryDefinitionWithExpr *>(result.value.get());
  ASSERT_NE(parsedDefinition, nullptr);
  EXPECT_EQ(parsedDefinition->name, "a");
  EXPECT_EQ(parsedDefinition->expr.get(), nullptr);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));
}

TEST(RecoveryTest, RepetitionAllowsInsertRetryAfterRewoundProgressAtEof) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("+"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryDefinitionWithExpr> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithExpr::name>(id) + ":"_kw +
          assign<&RecoveryDefinitionWithExpr::expr>(expressionRule) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(definition))};

  const auto result = parseRule(module, "module demo\n\ndef a : 3  +5\n", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_EQ(parsedModule->statements.size(), 1u);

  auto *parsedDefinition =
      dynamic_cast<RecoveryDefinitionWithExpr *>(parsedModule->statements.front().get());
  ASSERT_NE(parsedDefinition, nullptr);
  EXPECT_EQ(parsedDefinition->name, "a");

  auto *binary =
      dynamic_cast<RecoveryBinaryExpression *>(parsedDefinition->expr.get());
  ASSERT_NE(binary, nullptr);
  EXPECT_EQ(binary->op, "+");
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left.get());
  auto *right = dynamic_cast<RecoveryNumberExpression *>(binary->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->value, 3);
  EXPECT_EQ(right->value, 5);
}

TEST(RecoveryTest, OptionalBranchDoesNotEmitSpuriousInsertionsWhenSuffixMatches) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};

  const auto result = parseRule(definition, "def a :;", skipper);

  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Inserted);
  const auto *expectedElement = result.parseDiagnostics.front().element;
  ASSERT_NE(expectedElement, nullptr);
  if (expectedElement->getKind() == pegium::grammar::ElementKind::Assignment) {
    expectedElement =
        static_cast<const pegium::grammar::Assignment *>(expectedElement)
            ->getElement();
  }
  const auto *expectedRule =
      dynamic_cast<const pegium::grammar::AbstractRule *>(expectedElement);
  ASSERT_NE(expectedRule, nullptr);
  EXPECT_EQ(expectedRule->getName(), "Expression");
}

TEST(RecoveryTest,
     OptionalBranchDoesNotInventMissingPrefixBeforeDelimiterRecovery) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "def a: 5\ndef b: 3;", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const ParseDiagnostic &diagnostic) {
                                     return diagnostic.kind ==
                                            ParseDiagnosticKind::Deleted;
                                   }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_EQ(parsedModule->statements.size(), 2u);

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr);
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(firstDefinition->name, "a");
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(RecoveryTest, StartedOptionalBranchKeepsRecoveredArgumentPrefix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argument{
      "Argument", create<RecoveryNumberExpression>() +
                      assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() +
              assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argument) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(argument)) +
                     ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "sqrt(81/);", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                               ParseDiagnosticKind::Deleted &&
                                           diagnostic.offset == 7u;
                                  }));

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr);
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr);
  EXPECT_EQ(parsedCall->name, "sqrt");
  ASSERT_EQ(parsedCall->args.size(), 1u);

  auto *parsedArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args.front().get());
  ASSERT_NE(parsedArgument, nullptr);
  EXPECT_EQ(parsedArgument->value, 81);
}

TEST(RecoveryTest, UnexpectedPrimaryBeforeOperatorStillReportsSyntaxRecovery) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      append<&RecoveryModule::statements>(evaluation) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "1 2*3;", skipper);

  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.isSyntax();
                                  }));
}

TEST(RecoveryTest, UnexpectedTokenAfterOperatorUsesGenericDeleteInPrimary) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryDefinitionWithExpr> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithExpr::name>(id) + ":"_kw +
          assign<&RecoveryDefinitionWithExpr::expr>(expressionRule) + ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(definition | evaluationRule))};

  const auto result = parseRule(module,
                                "module calc\n"
                                "def c: 8;\n"
                                "2 * +c;\n",
                                skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Deleted;
                                  }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_EQ(parsedModule->statements.size(), 2u);

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1].get());
  ASSERT_NE(evaluationNode, nullptr);
  auto *binary =
      dynamic_cast<RecoveryBinaryExpression *>(evaluationNode->expression.get());
  ASSERT_NE(binary, nullptr);
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left.get());
  auto *right = dynamic_cast<RecoveryReferenceExpression *>(binary->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->value, 2);
  EXPECT_EQ(binary->op, "*");
  EXPECT_EQ(right->name, "c");
}

TEST(RecoveryTest, IncompleteDiagnosticAfterInfixOperatorUsesFailureTokenAnchor) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "module name\n2   *", skipper);

  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Incomplete);
  EXPECT_EQ(result.parseDiagnostics.front().offset, 17u);
}

TEST(RecoveryTest,
     RepetitionRecoveryDoesNotStopOnRewoundFailureInsideActiveWindow) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module,
                                "module calc\n"
                                "1+2;v\n"
                                "3+4;\n"
                                "5+6;\n",
                                skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_FALSE(result.parseDiagnostics.empty());

  auto *parsedModule =
      dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_GE(parsedModule->statements.size(), 3u);
}

TEST(RecoveryTest, LegacyOrderedChoiceOperatorRecoveryDoesNotThrow) {
  struct LegacyRecoveryParser final : PegiumParser {
    using PegiumParser::parse;

    TerminalRule<> ws{"WS", some(s)};
    Skipper skipper = SkipperBuilder().ignore(ws).build();

    TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
    TerminalRule<int> number{"NUMBER", some(d)};

    Rule<RecoveryExpression> legacyPrimary{
        "LegacyPrimary",
        create<RecoveryNumberExpression>() +
                assign<&RecoveryNumberExpression::value>(number) |
            create<RecoveryReferenceExpression>() +
                assign<&RecoveryReferenceExpression::name>(id)};
    Rule<RecoveryExpression> legacyExpression{
        "LegacyExpression",
        legacyPrimary +
            many(nest<&RecoveryBinaryExpression::left>() +
                 assign<&RecoveryBinaryExpression::op>(
                     "+"_kw | "-"_kw | "*"_kw) +
                 assign<&RecoveryBinaryExpression::right>(legacyPrimary))};
    Rule<RecoveryExpressionEvaluation> evaluation{
        "Evaluation",
        "legacy"_kw +
            assign<&RecoveryExpressionEvaluation::expression>(legacyExpression) +
            ";"_kw};

    [[nodiscard]] const pegium::grammar::ParserRule &
    getEntryRule() const noexcept override {
      return evaluation;
    }

    [[nodiscard]] const Skipper &getSkipper() const noexcept override {
      return skipper;
    }
  };

  LegacyRecoveryParser parser;

  for (const auto *text : {
           "legacy 1 + ;",
           "legacy 1 * ;",
           "legacy a + ;",
           "legacy 1 + * 2;",
           "legacy a * / b;",
           "legacy 1 +\n",
       }) {
    SCOPED_TRACE(text);
    EXPECT_NO_THROW({
      const auto result = parser.parse(text);
      EXPECT_GT(result.parsedLength, 0u);
    });
  }
}
