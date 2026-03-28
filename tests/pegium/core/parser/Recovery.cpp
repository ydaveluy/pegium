#include <gtest/gtest.h>
#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <algorithm>
#include <sstream>
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

struct RecoveryContact : pegium::AstNode {
  string name;
};

struct RecoveryFeatureNode : pegium::AstNode {
  bool many = false;
  string name;
  string type;
};

struct RecoveryFeatureListNode : pegium::AstNode {
  vector<pointer<RecoveryFeatureNode>> features;
};

struct RecoveryEnvironmentNode : pegium::AstNode {
  string name;
  string label;
};

struct RecoveryRequirementNode : pegium::AstNode {
  string name;
  string label;
};

struct RecoveryRequirementModelNode : pegium::AstNode {
  pointer<RecoveryContact> contact;
  vector<pointer<RecoveryEnvironmentNode>> environments;
  vector<pointer<RecoveryRequirementNode>> requirements;
};

struct RecoveryNameListNode : pegium::AstNode {
  vector<string> names;
};

struct RecoveryPrefixedNameListNode : pegium::AstNode {
  string name;
  string label;
  vector<string> names;
};

struct RecoveryTaggedRequirementListNode : pegium::AstNode {
  string name;
  string label;
  vector<string> environments;
};

struct RecoveryTaggedRequirementModelNode : pegium::AstNode {
  vector<pointer<RecoveryEnvironmentNode>> environments;
  vector<pointer<RecoveryTaggedRequirementListNode>> requirements;
};

struct RecoveryTransitionBlockNode : pegium::AstNode {};

struct RecoveryTransitionNode : pegium::AstNode {
  string event;
  string target;
};

struct RecoveryStateNode : pegium::AstNode {
  string name;
  vector<pointer<RecoveryTransitionNode>> transitions;
};

struct RecoveryStateModelNode : pegium::AstNode {
  vector<pointer<RecoveryStateNode>> states;
};

template <typename T> struct ValueNode : pegium::AstNode {
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

std::string dump_parse_diagnostics(
    const std::vector<pegium::parser::ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    dump += std::to_string(diagnostic.beginOffset);
    dump += "-";
    dump += std::to_string(diagnostic.endOffset);
    dump += ":";
    switch (diagnostic.kind) {
    case ParseDiagnosticKind::Inserted:
      dump += "Inserted";
      break;
    case ParseDiagnosticKind::Deleted:
      dump += "Deleted";
      break;
    case ParseDiagnosticKind::Replaced:
      dump += "Replaced";
      break;
    case ParseDiagnosticKind::Incomplete:
      dump += "Incomplete";
      break;
    case ParseDiagnosticKind::Recovered:
      dump += "Recovered";
      break;
    case ParseDiagnosticKind::ConversionError:
      dump += "ConversionError";
      break;
    }
    if (diagnostic.element != nullptr) {
      dump += ":";
      std::ostringstream oss;
      oss << *diagnostic.element;
      dump += oss.str();
    }
    if (!diagnostic.message.empty()) {
      dump += ":";
      dump += diagnostic.message;
    }
  }
  return dump;
}

} // namespace

TEST(RecoveryTest,
     WordLiteralReplacementRespectsConfidenceBeforeDeleteCostBudget) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const std::string input = "xxxxxxxxxxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  ParseOptions constrainedOptions;
  constrainedOptions.maxRecoveryEditCost = 64;
  const auto constrainedResult =
      parseDataType(rule, input, skipper, constrainedOptions);
  EXPECT_FALSE(constrainedResult.value);
  ASSERT_FALSE(constrainedResult.parseDiagnostics.empty());
  EXPECT_EQ(constrainedResult.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);

  ParseOptions tunedOptions;
  tunedOptions.maxRecoveryEditCost = 128;
  const auto tunedResult = parseDataType(rule, input, skipper, tunedOptions);
  EXPECT_TRUE(tunedResult.value);
  EXPECT_FALSE(tunedResult.parseDiagnostics.empty());
}

TEST(RecoveryTest, ContiguousDeleteRunCanRecoverBeyondDefaultEditCountBudget) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const std::string input = "xxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  const auto result = parseDataType(rule, input, skipper);

  ASSERT_TRUE(result.value);
  ASSERT_FALSE(result.parseDiagnostics.empty());
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Deleted);
  EXPECT_EQ(result.parseDiagnostics.front().beginOffset, 0u);
  EXPECT_EQ(result.parseDiagnostics.front().endOffset, 9u);
}

TEST(RecoveryTest,
     NullableRepetitionCanYieldToStrictSuffixAfterFalseIterationStart) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionBlockNode> rule{"Root", many(id + "=>"_kw + id) +
                                                           "end"_kw};
  const auto skipper = SkipperBuilder().build();

  const auto result = parseRule(rule, "end", skipper);

  EXPECT_TRUE(result.fullMatch);
  EXPECT_EQ(result.parsedLength, 3u);
  EXPECT_TRUE(result.parseDiagnostics.empty())
      << dump_parse_diagnostics(result.parseDiagnostics);
}

TEST(RecoveryTest,
     BoundedNullableRepetitionCanYieldToStrictSuffixAfterFalseIterationStart) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Root", repeat<0, 3>(id + "=>"_kw + id) + "end"_kw};
  const auto skipper = SkipperBuilder().build();

  const auto result = parseRule(rule, "end", skipper);

  EXPECT_TRUE(result.fullMatch);
  EXPECT_EQ(result.parsedLength, 3u);
  EXPECT_TRUE(result.parseDiagnostics.empty())
      << dump_parse_diagnostics(result.parseDiagnostics);
}

TEST(RecoveryTest, DiagnosticsTrackDeleteAndInsertEdits) {
  const auto skipper = SkipperBuilder().build();

  {
    DataTypeRule<std::string> rule{"Rule", "service"_kw};
    const std::string input = "oopsservice";
    const auto result = parseDataType(rule, input, skipper);

    ASSERT_TRUE(result.value);
    ASSERT_FALSE(result.parseDiagnostics.empty());
    EXPECT_TRUE(std::ranges::all_of(
        result.parseDiagnostics, [](const ParseDiagnostic &d) {
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
    EXPECT_TRUE(std::ranges::any_of(
        result.parseDiagnostics, [](const ParseDiagnostic &d) {
          return d.kind == ParseDiagnosticKind::Inserted;
        }));
    const auto inserted = std::ranges::find_if(
        result.parseDiagnostics, [](const ParseDiagnostic &d) {
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

  const auto result =
      parseDataType(rule, "", SkipperBuilder().build(), options);

  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);
  EXPECT_EQ(result.parseDiagnostics.front().offset, 0u);
}

TEST(RecoveryTest, GenericLiteralFuzzyRecoveryRepairsSingleEditOsaShapes) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const auto skipper = SkipperBuilder().build();

  {
    const std::string input = "servixe";
    const auto result = parseDataType(rule, input, skipper);
    EXPECT_TRUE(result.value);
    EXPECT_TRUE(std::ranges::any_of(
        result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
          return diagnostic.kind == ParseDiagnosticKind::Replaced;
        }));
  }

  {
    const std::string input = "serivce";
    const auto result = parseDataType(rule, input, skipper);
    EXPECT_TRUE(result.value);
    EXPECT_TRUE(std::ranges::any_of(
        result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
          return diagnostic.kind == ParseDiagnosticKind::Replaced;
        }));
  }

  {
    const std::string input = "sxrivxe";
    const auto result = parseDataType(rule, input, skipper);
    EXPECT_TRUE(result.value);
    EXPECT_TRUE(std::ranges::any_of(
        result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
          return diagnostic.kind == ParseDiagnosticKind::Replaced;
        }));
  }
}

TEST(RecoveryTest, AnyCharacterRecoveryDoesNotInventMissingCharacter) {
  DataTypeRule<std::string_view> rule{"Rule", dot};
  const auto result = parseDataType(rule, "", SkipperBuilder().build());

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(std::ranges::none_of(
      result.parseDiagnostics, [](const ParseDiagnostic &d) {
        return d.kind == ParseDiagnosticKind::Inserted;
      }));
}

TEST(RecoveryTest, AndPredicateRecoveryDoesNotUseEdits) {
  DataTypeRule<std::string> rule{"Rule", &"a"_kw + "a"_kw};
  const std::string input = "xa";
  const auto result = parseDataType(rule, input, SkipperBuilder().build());
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_FALSE(result.value) << parseDump;
}

TEST(RecoveryTest, NotPredicateRecoveryDoesNotUsePrefixDeleteRetry) {
  DataTypeRule<std::string> rule{"Rule", !"x"_kw + "a"_kw};
  const std::string input = "xa";
  const auto result = parseDataType(rule, input, SkipperBuilder().build());
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  EXPECT_FALSE(result.value) << parseDump;
}

TEST(RecoveryTest,
     OrderedChoiceRecoveryDoesNotShortCircuitCleanBoundaryWhenLaterBranchWins) {
  const auto skipper = SkipperBuilder().build();
  ParserRule<RecoveryNode> rule{"Rule", "a"_kw | ("ab"_kw + ";"_kw)};

  const auto result = parseRule(rule, "ab", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  ASSERT_EQ(result.parseDiagnostics.size(), 1u) << parseDump;
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Inserted)
      << parseDump;
}

TEST(RecoveryTest, RecoveryCanBeDisabledThroughParseOptions) {
  const std::string input = "oopsservice";
  const auto skipper = SkipperBuilder().build();

  ParseOptions options;
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

TEST(
    RecoveryTest,
    ConsecutiveDeleteRecoveryAcrossTerminalBoundaryIsNotGuaranteedGenerically) {
  DataTypeRule<std::string> rule{"Rule", "aaa"_kw + "{"_kw};
  const std::string input = "aaaXXX{";
  const auto skipper = SkipperBuilder().build();

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 8;

  const auto result = parseDataType(rule, input, skipper, options);
  EXPECT_FALSE(result.value);
}

TEST(RecoveryTest,
     MissingRequiredTokenRecoveryStillBuildsRootAndReportsSyntax) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<RecoveryEvaluation> evaluation{
      "Evaluation", assign<&RecoveryEvaluation::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module \n\ndef a:4;", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_FALSE(result.parseDiagnostics.empty());
  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [](const ParseDiagnostic &diagnostic) { return diagnostic.isSyntax(); }));
}

TEST(RecoveryTest,
     WordBoundaryViolationCanInsertSyntheticGapForKeywordLiteral) {
  const auto skipper = SkipperBuilder().build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "modulebasicMath", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted;
      }));
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }));

  const auto inserted = std::ranges::find_if(
      result.parseDiagnostics, [](const ParseDiagnostic &d) {
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
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      }));

  const auto replaced = std::ranges::find_if(
      result.parseDiagnostics, [](const ParseDiagnostic &d) {
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
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest, HiddenGapWordLikeLiteralCanUseIdentifierLikeFuzzyRepair) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "prefix"_kw + "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "prefix\nmodle basicMath", skipper);

  ASSERT_TRUE(result.value) << dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest,
     HiddenGapWordLikeLiteralDoesNotFuzzyReplaceFromPunctuationSource) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryModule> module{
      "Module", "prefix"_kw + "module"_kw + assign<&RecoveryModule::name>(id)};

  const auto result = parseRule(module, "prefix\n:odule basicMath", skipper);

  EXPECT_FALSE(result.value);
  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);
}

TEST(RecoveryTest,
     WordLikeLiteralFuzzyAdmissibilityUsesSharedLexicalAndTriviaFacts) {
  const auto profile = detail::classify_literal_recovery_profile("module");
  detail::LiteralFuzzyCandidate candidate{
      .consumed = 6u,
      .distance = 2u,
      .operationCount = 2u,
      .cost = detail::make_recovery_cost(2u, 4u, 4u),
      .substitutionCount = 1u,
  };
  const detail::TerminalRecoveryFacts hiddenGapFacts{
      .triviaGap = {.hiddenCodepointSpan = 1u,
                    .visibleSourceAfterLocalSkip = false},
      .previousElementIsTerminalish = false,
  };

  EXPECT_FALSE(detail::allows_literal_fuzzy_candidate(candidate, profile,
                                                      hiddenGapFacts, false));
  EXPECT_FALSE(detail::allows_literal_fuzzy_candidate(candidate, profile,
                                                      hiddenGapFacts, true));

  candidate.substitutionCount = 0u;
  EXPECT_TRUE(detail::allows_literal_fuzzy_candidate(candidate, profile,
                                                     hiddenGapFacts, true));

  candidate.cost.primaryRankCost = 10u;
  EXPECT_FALSE(
      detail::allows_literal_fuzzy_candidate(candidate, profile, {}, true));
}

TEST(
    RecoveryTest,
    OperatorLikeLiteralFuzzyAdmissibilityAllowsOnlySingleNonSubstitutiveRepair) {
  const auto profile = detail::classify_literal_recovery_profile("=>");
  detail::LiteralFuzzyCandidate candidate{
      .consumed = 2u,
      .distance = 1u,
      .operationCount = 1u,
      .cost = detail::make_recovery_cost(1u, 1u, 1u),
      .substitutionCount = 0u,
  };

  EXPECT_TRUE(
      detail::allows_literal_fuzzy_candidate(candidate, profile, {}, false));

  candidate.substitutionCount = 1u;
  EXPECT_FALSE(
      detail::allows_literal_fuzzy_candidate(candidate, profile, {}, false));

  candidate.substitutionCount = 0u;
  candidate.operationCount = 2u;
  EXPECT_FALSE(
      detail::allows_literal_fuzzy_candidate(candidate, profile, {}, false));

  candidate.operationCount = 1u;
  candidate.distance = 2u;
  EXPECT_FALSE(
      detail::allows_literal_fuzzy_candidate(candidate, profile, {}, false));
}

TEST(RecoveryTest,
     OperatorLikeTerminalDisallowsNearbyDeleteScanAfterWideHiddenGap) {
  auto skipper = SkipperBuilder().build();
  const auto matchArrow = [](const char *scanCursor) noexcept {
    return "=>"_kw.terminal(scanCursor);
  };
  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("xy=>");
    auto &builder = builderHarness.builder;
    detail::FailureHistoryRecorder recorder(builder.input_begin());
    RecoveryContext ctx{builder, skipper, recorder};
    ctx.skip();
    EXPECT_TRUE(detail::probe_nearby_delete_scan_match(ctx, matchArrow));
  }

  auto hiddenGapHarness = pegium::test::makeCstBuilderHarness("xy=>");
  auto &hiddenGapBuilder = hiddenGapHarness.builder;
  detail::FailureHistoryRecorder hiddenGapRecorder(
      hiddenGapBuilder.input_begin());
  RecoveryContext hiddenGapCtx{hiddenGapBuilder, skipper, hiddenGapRecorder};
  hiddenGapCtx.skip();
  const detail::TerminalRecoveryFacts facts{
      .triviaGap = {.hiddenCodepointSpan = 12u,
                    .visibleSourceAfterLocalSkip = false},
      .previousElementIsTerminalish = false,
  };
  EXPECT_FALSE(detail::probe_nearby_delete_scan_match(
      hiddenGapCtx, matchArrow, facts,
      detail::classify_literal_recovery_profile("=>")));
}

TEST(RecoveryTest,
     OperatorLikeTerminalAllowsNearbyDeleteScanAfterCompactHiddenGap) {
  auto skipper = SkipperBuilder().build();
  const auto matchArrow = [](const char *scanCursor) noexcept {
    return "=>"_kw.terminal(scanCursor);
  };

  auto hiddenGapHarness = pegium::test::makeCstBuilderHarness("xy=>");
  auto &hiddenGapBuilder = hiddenGapHarness.builder;
  detail::FailureHistoryRecorder hiddenGapRecorder(
      hiddenGapBuilder.input_begin());
  RecoveryContext hiddenGapCtx{hiddenGapBuilder, skipper, hiddenGapRecorder};
  hiddenGapCtx.skip();
  const detail::TerminalRecoveryFacts facts{
      .triviaGap = {.hiddenCodepointSpan = 3u,
                    .visibleSourceAfterLocalSkip = false},
      .previousElementIsTerminalish = false,
  };
  EXPECT_TRUE(detail::probe_nearby_delete_scan_match(
      hiddenGapCtx, matchArrow, facts,
      detail::classify_literal_recovery_profile("=>")));
}

TEST(RecoveryTest,
     SeparatorTerminalDisallowsNearbyDeleteScanAfterCompactHiddenGap) {
  auto skipper = SkipperBuilder().build();
  const auto matchSemicolon = [](const char *scanCursor) noexcept {
    return ";"_kw.terminal(scanCursor);
  };

  auto hiddenGapHarness = pegium::test::makeCstBuilderHarness("x;");
  auto &hiddenGapBuilder = hiddenGapHarness.builder;
  detail::FailureHistoryRecorder hiddenGapRecorder(
      hiddenGapBuilder.input_begin());
  RecoveryContext hiddenGapCtx{hiddenGapBuilder, skipper, hiddenGapRecorder};
  hiddenGapCtx.skip();
  const detail::TerminalRecoveryFacts facts{
      .triviaGap = {.hiddenCodepointSpan = 1u,
                    .visibleSourceAfterLocalSkip = false},
      .previousElementIsTerminalish = false,
  };
  EXPECT_FALSE(detail::probe_nearby_delete_scan_match(
      hiddenGapCtx, matchSemicolon, facts,
      detail::classify_literal_recovery_profile(";")));
}

TEST(RecoveryTest,
     TerminalRecoveryFactsRestrictDeleteScanBetweenAdjacentTerminals) {
  auto skipper = SkipperBuilder().build();
  const auto matchArrow = [](const char *scanCursor) noexcept {
    return "=>"_kw.terminal(scanCursor);
  };
  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("xy=>");
    auto &builder = builderHarness.builder;
    detail::FailureHistoryRecorder recorder(builder.input_begin());
    RecoveryContext ctx{builder, skipper, recorder};
    ctx.skip();
    EXPECT_TRUE(detail::probe_nearby_delete_scan_match(ctx, matchArrow));
  }

  auto restrictedHarness = pegium::test::makeCstBuilderHarness("xy=>");
  auto &restrictedBuilder = restrictedHarness.builder;
  detail::FailureHistoryRecorder restrictedRecorder(
      restrictedBuilder.input_begin());
  RecoveryContext restrictedCtx{restrictedBuilder, skipper, restrictedRecorder};
  restrictedCtx.skip();
  const detail::TerminalRecoveryFacts facts{
      .triviaGap = {.hiddenCodepointSpan = 0u,
                    .visibleSourceAfterLocalSkip = false},
      .previousElementIsTerminalish = true,
  };
  EXPECT_FALSE(
      detail::probe_nearby_delete_scan_match(restrictedCtx, matchArrow, facts));
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
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest, SymbolicLiteralOnlyAllowsSingleNonSubstitutiveFuzzyRepair) {
  const auto skipper = SkipperBuilder().build();
  DataTypeRule<std::string> arrow{"Arrow", "=>"_kw};

  const auto missingCharacter = parseDataType(arrow, ">", skipper);
  ASSERT_TRUE(missingCharacter.value);
  EXPECT_TRUE(missingCharacter.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(
      missingCharacter.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      }));

  const auto substitutedCharacter = parseDataType(arrow, "->", skipper);
  EXPECT_FALSE(substitutedCharacter.value);
  ASSERT_FALSE(substitutedCharacter.parseDiagnostics.empty());
  EXPECT_EQ(substitutedCharacter.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);
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
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  EXPECT_EQ(parsedModule->name, "basicMath");
}

TEST(RecoveryTest,
     ParserRuleRejectsLowConfidenceReplacementBeforeDeleteCostBudget) {
  const std::string input = "xxxxxxxxxxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  ParserRule<RecoveryNode> rule{"ParserRule",
                                assign<&RecoveryNode::token>("service"_kw)};
  ParseOptions constrainedOptions;
  constrainedOptions.maxRecoveryEditCost = 64;
  const auto constrainedResult =
      parseRule(rule, input, skipper, constrainedOptions);
  EXPECT_FALSE(constrainedResult.value);
  ASSERT_FALSE(constrainedResult.parseDiagnostics.empty());
  EXPECT_EQ(constrainedResult.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);

  ParseOptions tunedOptions;
  tunedOptions.maxRecoveryEditCost = 128;
  const auto tunedResult = parseRule(rule, input, skipper, tunedOptions);
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
      "Expression", create<RecoveryNumberExpression>() +
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
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted;
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
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "module demo\n\ndef a : 3  +5\n", skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted;
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_EQ(parsedModule->statements.size(), 1u);

  auto *parsedDefinition = dynamic_cast<RecoveryDefinitionWithExpr *>(
      parsedModule->statements.front().get());
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

TEST(RecoveryTest,
     OptionalBranchDoesNotEmitSpuriousInsertionsWhenSuffixMatches) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
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
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Inserted);
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
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
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
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted;
      }));
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
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

TEST(RecoveryTest,
     OptionalBranchDoesNotBeatMissingDelimiterInsertInFollowingSequenceTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "def a 5;\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted;
      })) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  ASSERT_NE(firstDefinition->expr, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingDelimiterInsertStillWinsInsideStatementChoiceSequence) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
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

  const auto result =
      parseRule(module, "def a 5;\ndef b: 3;\nb;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted;
      })) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  auto *thirdEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[2].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  ASSERT_NE(thirdEvaluation, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingDelimiterInsertStillWinsInsideModuleWithStatementChoice) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
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
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a 5;\ndef b: 3;\nb;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingDelimiterInsertStillWinsInsideModuleWithDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "module m\ndef a 5;\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingDelimiterInsertStillWinsInsideModuleWithNonCompetingStatementChoice) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryEvaluation> impossible{
      "Impossible", "value"_kw + assign<&RecoveryEvaluation::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | impossible};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a 5;\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStillWinsInsideModuleWithStatementChoice) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
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
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStillWinsInsideModuleWithDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingColonInsertionsStayLocalAcrossLaterRecoveryWindowsInDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result = parseRule(module, "module m\ndef a 5;\ndef b 3;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  const auto insertedColonCount = std::ranges::count_if(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal &&
               diagnostic.beginOffset == diagnostic.endOffset;
      });
  EXPECT_EQ(insertedColonCount, 2) << parseDump;
  EXPECT_TRUE(std::ranges::all_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        if (!isSyntaxParseDiagnostic(diagnostic.kind)) {
          return true;
        }
        return diagnostic.kind == ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal &&
               diagnostic.beginOffset == diagnostic.endOffset;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;

  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  auto *secondNumber =
      dynamic_cast<RecoveryNumberExpression *>(secondDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  ASSERT_NE(secondNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
  EXPECT_EQ(secondNumber->value, 3) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertKeepsFollowingDefinitionInsideStatementChoiceBeforeTrailingGarbage) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
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
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\n;\n;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertKeepsFollowingDefinitionsBeforeTrailingGarbageInDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\n;\n;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
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
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option(
                  "("_kw + append<&RecoveryFunctionCall::args>(argument) +
                  many(","_kw + append<&RecoveryFunctionCall::args>(argument)) +
                  ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "sqrt(81/);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.offset == 7u;
      }))
      << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "sqrt") << parseDump;
  ASSERT_EQ(parsedCall->args.size(), 1u) << parseDump;

  auto *parsedArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args.front().get());
  ASSERT_NE(parsedArgument, nullptr) << parseDump;
  EXPECT_EQ(parsedArgument->value, 81) << parseDump;
}

TEST(RecoveryTest, StartedOptionalBranchWithEmptyCallKeepsCallPrefix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argument{
      "Argument", create<RecoveryNumberExpression>() +
                      assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option(
                  "("_kw + append<&RecoveryFunctionCall::args>(argument) +
                  many(","_kw + append<&RecoveryFunctionCall::args>(argument)) +
                  ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "sqrt();", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "sqrt") << parseDump;
  EXPECT_TRUE(parsedCall->args.empty()) << parseDump;
}

TEST(RecoveryTest, StartedOptionalBranchWithMissingCommaKeepsCallPrefix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argument{
      "Argument", create<RecoveryNumberExpression>() +
                      assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option(
                  "("_kw + append<&RecoveryFunctionCall::args>(argument) +
                  many(","_kw + append<&RecoveryFunctionCall::args>(argument)) +
                  ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "root(64 3);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "root") << parseDump;
  ASSERT_EQ(parsedCall->args.size(), 2u) << parseDump;

  auto *firstArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args[0].get());
  auto *secondArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args[1].get());
  ASSERT_NE(firstArgument, nullptr) << parseDump;
  ASSERT_NE(secondArgument, nullptr) << parseDump;
  EXPECT_EQ(firstArgument->value, 64) << parseDump;
  EXPECT_EQ(secondArgument->value, 3) << parseDump;
}

TEST(RecoveryTest, InfixArgumentCallKeepsFunctionNameWhenCommaIsMissing) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "root(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "root") << parseDump;
  ASSERT_EQ(parsedCall->args.size(), 2u) << parseDump;
}

TEST(RecoveryTest, ArithmeticShapedModuleKeepsBrokenCallLocal) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};
  const std::string input =
      "module basicMath\n"
      "\n"
      "Root(64 3/0); // 4\n";

  const auto result = parseRule(module, input, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.front().get());
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(evaluationNode->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
  ASSERT_EQ(call->args.size(), 2u) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsComplexBrokenCallLocalAfterSplitGarbageBlock) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw) |
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "xx\n"
                                "xxxxxxxxxxxx;\n"
                                "xx\n"
                                "\n"
                                "Root(64 + 5 3/0+5-3); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_FALSE(parsedModule->statements.empty()) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
  EXPECT_FALSE(call->args.empty()) << parseDump;
}

TEST(RecoveryTest, ArithmeticShapedModuleKeepsBrokenCallLocalWithoutComment) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "Root(64 3/0);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsBrokenCallLocalWithTrailingCommentAndSlashOnly) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression", primaryExpression,
                       LeftAssociation("/"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "Root(64 3/0); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsBrokenCallLocalWithTrailingCommentAndReferenceBranch) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression", primaryExpression,
                       LeftAssociation("/"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw) |
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "Root(64 3/0); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedEvaluationKeepsBrokenCallLocalWithTrailingComment) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression", primaryExpression,
                       LeftAssociation("/"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};

  const auto result = parseRule(evaluation, "Root(64 3/0); // 4\n", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     StatementListKeepsFunctionNameForFollowingBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "xx;\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_FALSE(parsedModule->statements.empty()) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr) << parseDump;
  EXPECT_EQ(lastCall->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     StatementChoiceKeepsFunctionNameForFollowingBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "def a:1;\nxx;\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr) << parseDump;
  EXPECT_EQ(lastCall->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     ZeroMinStatementChoiceKeepsFunctionNameForBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module m\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.front().get());
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(evaluationNode->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     ZeroMinStatementChoiceKeepsFunctionNameForBrokenInfixArgumentCallWithTrailingComment) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module m\nRoot(64 3/0); // 4\n", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     RecursiveArgumentExpressionKeepsFunctionNameWhenCommaIsMissing) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryReferenceExpression>() +
                     assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression", primary, LeftAssociation("/"_kw)};
  expression = binaryExpression;
  primary =
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id) |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number);
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module m\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.front().get());
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(evaluationNode->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStaysInsertOnlyAcrossRepeatedEvaluationTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module,
                                "module m\n"
                                "Root(64,3);\n"
                                "Root(64,3)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 5u) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStaysInsertOnlyAcrossRepeatedStatementChoiceTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module m\n"
                                "def a:1;\n"
                                "Root(64,3);\n"
                                "Root(64,3)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 6u) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStaysInsertOnlyAcrossRepeatedStatementChoiceTailWithHiddenComments) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module m\n"
                                "def a:1;\n"
                                "Root(64,3); // 4\n"
                                "Root(64,3) // 4\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81); // 9\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 6u) << parseDump;
}

TEST(RecoveryTest,
     LongMissingSemicolonInsertStaysInsertOnlyAcrossRepeatedStatementChoiceTailWithHiddenComments) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module m\n"
                                "def a:1;\n"
                                "Root(64,3); // 4\n"
                                "Root(64,3) // 4\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81); // 9\n"
                                "Sqrt(81); // 9\n"
                                "Sqrt(81); // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81); // 9\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  EXPECT_GE(parsedModule->statements.size(), 18u) << parseDump;
}

TEST(RecoveryTest,
     LongMissingSemicolonInsertStaysInsertOnlyAcrossRepeatedEvaluationTailWithHiddenComments) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module,
                                "module m\n"
                                "Root(64,3); // 4\n"
                                "Root(64,3) // 4\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81); // 9\n"
                                "Sqrt(81); // 9\n"
                                "Sqrt(81); // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81); // 9\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  EXPECT_GE(parsedModule->statements.size(), 17u) << parseDump;
}

TEST(RecoveryTest,
     StatementChoiceWithHiddenCommentKeepsFunctionNameForBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "def a:1;\n"
                                "xx;\n"
                                "Root(64 3/0); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr) << parseDump;
  EXPECT_EQ(lastCall->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     OptionalStartedHeaderRecoversUnexpectedColonBeforeRequiredString) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryContact> contact{
      "Contact", "contact"_kw + ":"_kw + assign<&RecoveryContact::name>(text)};
  ParserRule<RecoveryEnvironmentNode> environment{
      "Environment", "environment"_kw +
                         assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                         assign<&RecoveryEnvironmentNode::label>(text)};
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id) +
                         assign<&RecoveryRequirementNode::label>(text)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      option(assign<&RecoveryRequirementModelNode::contact>(contact)) +
          many(append<&RecoveryRequirementModelNode::environments>(
              environment)) +
          some(append<&RecoveryRequirementModelNode::requirements>(
              requirement))};

  const auto result = parseRule(model,
                                "contact:: \"team\"\n"
                                "environment prod: \"Production\"\n"
                                "req login \"Users can login\"\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Deleted;
                                  }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_NE(parsedModel->contact, nullptr) << parseDump;
  EXPECT_EQ(parsedModel->contact->name, "\"team\"");
  ASSERT_EQ(parsedModel->environments.size(), 1u) << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
}

TEST(RecoveryTest,
     OptionalStartedHeadersKeepFollowingHeadersRecoverableAcrossWindows) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryContact> contact{
      "Contact", "contact"_kw + ":"_kw + assign<&RecoveryContact::name>(text)};
  ParserRule<RecoveryEnvironmentNode> environment{
      "Environment", "environment"_kw +
                         assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                         assign<&RecoveryEnvironmentNode::label>(text)};
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id) +
                         assign<&RecoveryRequirementNode::label>(text)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      option(assign<&RecoveryRequirementModelNode::contact>(contact)) +
          many(append<&RecoveryRequirementModelNode::environments>(
              environment)) +
          some(append<&RecoveryRequirementModelNode::requirements>(
              requirement))};

  const std::string input = "contact:: \"team\"\n"
                            "environment prod \"Production\"\n"
                            "environment staging:: \"Staging\"\n"
                            "req login \"Users can login\"\n";
  const auto result = parseRule(model, input, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diagnostic) {
                                     return diagnostic.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_NE(parsedModel->contact, nullptr) << parseDump;
  EXPECT_EQ(parsedModel->contact->name, "\"team\"") << parseDump;
  ASSERT_EQ(parsedModel->environments.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->environments[0]->name, "prod") << parseDump;
  EXPECT_EQ(parsedModel->environments[1]->name, "staging") << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->name, "login") << parseDump;
}

TEST(RecoveryTest,
     LongGarbagePrefixBeforeFirstRequirementCanDeleteScanIntoEntryRule) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id) +
                         assign<&RecoveryRequirementNode::label>(text)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      some(append<&RecoveryRequirementModelNode::requirements>(requirement))};

  const std::string input = "<<<<<<<<<<<<<<<<<<<<<<<<\n"
                            "req login \"Users can login\"\n";
  const auto result = parseRule(model, input, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diagnostic) {
                                     return diagnostic.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  const auto garbageBegin = input.find("<<<<<<<<<<<<<<<<<<<<<<<<");
  ASSERT_NE(garbageBegin, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [garbageBegin](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(garbageBegin);
      }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->name, "login") << parseDump;
}

TEST(RecoveryTest, OptionalTailListRecoversMissingCommaBetweenItems) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNameListNode> model{
      "Model",
      "root"_kw + create<RecoveryNameListNode>() +
          option("tail"_kw + append<&RecoveryNameListNode::names>(id) +
                 many(","_kw + append<&RecoveryNameListNode::names>(id)))};

  const auto result = parseRule(model, "root tail alpha beta", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryNameListNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->names.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->names[0], "alpha");
  EXPECT_EQ(parsedModel->names[1], "beta");
}

TEST(RecoveryTest,
     ZeroMinTopLevelRepetitionDoesNotStopBeforeRecoverableDefinitionKeyword) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> reference{
      "Reference",
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(reference) +
          ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "de a: 5;\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *parsedDefinition =
      dynamic_cast<RecoveryDefinition *>(parsedModule->statements.front().get());
  ASSERT_NE(parsedDefinition, nullptr) << parseDump;
  EXPECT_EQ(parsedDefinition->name, "a");
  EXPECT_EQ(parsedDefinition->value, 5);
}

TEST(RecoveryTest, OptionalTailListRecoversExtraCommaBeforeItem) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNameListNode> model{
      "Model",
      "root"_kw + create<RecoveryNameListNode>() +
          option("tail"_kw + append<&RecoveryNameListNode::names>(id) +
                 many(","_kw + append<&RecoveryNameListNode::names>(id)))};

  const auto result = parseRule(model, "root tail alpha,, beta", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Deleted;
                                  }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryNameListNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->names.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->names[0], "alpha");
  EXPECT_EQ(parsedModel->names[1], "beta");
}

TEST(RecoveryTest, LongPrefixOptionalTailListRecoversMissingCommaBetweenItems) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryPrefixedNameListNode> model{
      "Model",
      "root"_kw + create<RecoveryPrefixedNameListNode>() +
          assign<&RecoveryPrefixedNameListNode::name>(id) +
          assign<&RecoveryPrefixedNameListNode::label>(text) +
          option(
              "tail"_kw + append<&RecoveryPrefixedNameListNode::names>(id) +
              many(","_kw + append<&RecoveryPrefixedNameListNode::names>(id)))};

  const auto result = parseRule(
      model, "root login \"Users can login\" tail alpha beta", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;
}

TEST(RecoveryTest, DirectApplicableListRecoversMissingCommaBetweenItems) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTaggedRequirementListNode> applicable{
      "Applicable",
      "applicable"_kw + "for"_kw + create<RecoveryTaggedRequirementListNode>() +
          append<&RecoveryTaggedRequirementListNode::environments>(id) +
          many(","_kw +
               append<&RecoveryTaggedRequirementListNode::environments>(id))};

  const auto result =
      parseRule(applicable, "applicable for prod staging", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;
}

TEST(RecoveryTest,
     RequirementModelRecoversMissingCommaInsideOptionalApplicableTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryEnvironmentNode> environment{
      "Environment", "environment"_kw + create<RecoveryEnvironmentNode>() +
                         assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                         assign<&RecoveryEnvironmentNode::label>(text)};
  ParserRule<RecoveryTaggedRequirementListNode> requirement{
      "Requirement",
      "req"_kw + create<RecoveryTaggedRequirementListNode>() +
          assign<&RecoveryTaggedRequirementListNode::name>(id) +
          assign<&RecoveryTaggedRequirementListNode::label>(text) +
          option("applicable"_kw + "for"_kw +
                 append<&RecoveryTaggedRequirementListNode::environments>(id) +
                 many(","_kw +
                      append<&RecoveryTaggedRequirementListNode::environments>(
                          id)))};
  ParserRule<RecoveryTaggedRequirementModelNode> model{
      "Model",
      create<RecoveryTaggedRequirementModelNode>() +
          many(append<&RecoveryTaggedRequirementModelNode::environments>(
              environment)) +
          some(append<&RecoveryTaggedRequirementModelNode::requirements>(
              requirement))};

  const auto result =
      parseRule(model,
                "environment prod: \"Production\"\n"
                "environment staging: \"Staging\"\n"
                "req login \"Users can login\" applicable for prod staging",
                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryTaggedRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr);
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  ASSERT_NE(parsedModel->requirements[0], nullptr);
  ASSERT_EQ(parsedModel->requirements[0]->environments.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->environments[0], "prod") << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->environments[1], "staging")
      << parseDump;
}

TEST(RecoveryTest, OptionalInfixExpressionKeepsRecoveredPrefixBeforeDelimiter) {
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
      "Evaluation", option(assign<&RecoveryExpressionEvaluation::expression>(
                        expressionRule)) +
                        ";"_kw};

  const auto result = parseRule(evaluation, "81/;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                               ParseDiagnosticKind::Deleted &&
                                           diagnostic.offset == 2u;
                                  }))
      << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  ASSERT_NE(parsedEvaluation->expression, nullptr) << parseDump;
  auto *parsedNumber = dynamic_cast<RecoveryNumberExpression *>(
      parsedEvaluation->expression.get());
  ASSERT_NE(parsedNumber, nullptr) << parseDump;
  EXPECT_EQ(parsedNumber->value, 81);
}

TEST(RecoveryTest, InfixRuleProbeCanSeeStartedPrimaryWithMissingRhs) {
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};

  auto builderHarness = pegium::test::makeCstBuilderHarness("81/");
  auto &builder = builderHarness.builder;
  const auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  RecoveryContext ctx{builder, skipper, recorder};

  ctx.skip();

  EXPECT_TRUE(probe_locally_recoverable(expression, ctx));
  EXPECT_TRUE(probe_locally_recoverable(expressionRule, ctx));
}

TEST(RecoveryTest,
     MissingRequiredExpressionCanRecoverBeforeLongPunctuationRunAndDelimiter) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const std::string operatorRun(37u, '*');
  const auto result = parseRule(evaluation, operatorRun + ";", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [&](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset == 0u &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(operatorRun.size());
      }))
      << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
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
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", append<&RecoveryModule::statements>(evaluation) +
                    many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "1 2*3;", skipper);

  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [](const ParseDiagnostic &diagnostic) { return diagnostic.isSyntax(); }));
}

TEST(RecoveryTest, UnexpectedTokenAfterOperatorUsesGenericDeleteInPrimary) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
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
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition |
                                                             evaluationRule))};

  const auto result = parseRule(module,
                                "module calc\n"
                                "def c: 8;\n"
                                "2 * +c;\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1].get());
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *binary = dynamic_cast<RecoveryBinaryExpression *>(
      evaluationNode->expression.get());
  ASSERT_NE(binary, nullptr) << parseDump;
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left.get());
  auto *right =
      dynamic_cast<RecoveryReferenceExpression *>(binary->right.get());
  ASSERT_NE(left, nullptr) << parseDump;
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(left->value, 2) << parseDump;
  EXPECT_EQ(binary->op, "*") << parseDump;
  EXPECT_EQ(right->name, "c") << parseDump;
}

TEST(RecoveryTest,
     LongDeleteRunAfterOperatorKeepsNextStatementOutsideRecoveredExpression) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
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
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition |
                                                             evaluationRule))};

  const std::string text = "module calc\n"
                           "def a: 5;\n"
                           "\n"
                           "2*7+++++++++;\n"
                           "\n"
                           "def b: 5;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  const auto plusPos = text.find("+++++++++");
  ASSERT_NE(plusPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [plusPos](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(plusPos) &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(
                       plusPos + std::string_view{"+++++++++"}.size());
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1].get());
  ASSERT_NE(evaluationNode, nullptr);
  auto *binary = dynamic_cast<RecoveryBinaryExpression *>(
      evaluationNode->expression.get());
  ASSERT_NE(binary, nullptr);
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left.get());
  auto *right = dynamic_cast<RecoveryNumberExpression *>(binary->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(left->value, 2);
  EXPECT_EQ(binary->op, "*");
  EXPECT_EQ(right->value, 7);

  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithExpr *>(
      parsedModule->statements[2].get());
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(
    RecoveryTest,
    LongDeleteRunBeyondDefaultBudgetKeepsNextStatementOutsideRecoveredExpression) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
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
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition |
                                                             evaluationRule))};

  const std::string text = "module calc\n"
                           "def a: 5;\n"
                           "\n"
                           "2*7+++++++++++++++++++++++++++++++++++;\n"
                           "\n"
                           "def b: 5;\n";
  const auto result = parseRule(module, text, skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  const auto plusPos = text.find("+++++++++++++++++++++++++++++++++++");
  ASSERT_NE(plusPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [plusPos](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(plusPos) &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(
                       plusPos +
                       std::string_view{"+++++++++++++++++++++++++++++++++++"}
                           .size());
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_EQ(parsedModule->statements.size(), 3u);

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1].get());
  ASSERT_NE(evaluationNode, nullptr);
  auto *binary = dynamic_cast<RecoveryBinaryExpression *>(
      evaluationNode->expression.get());
  ASSERT_NE(binary, nullptr);
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left.get());
  auto *right = dynamic_cast<RecoveryNumberExpression *>(binary->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->value, 2);
  EXPECT_EQ(binary->op, "*");
  EXPECT_EQ(right->value, 7);

  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithExpr *>(
      parsedModule->statements[2].get());
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(
    RecoveryTest,
    LongOperatorRunWithoutDelimiterKeepsMultipleFollowingStatementsOutsideRecoveredExpression) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw), LeftAssociation("%"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluationRule))};

  const std::string operatorRun(95u, '*');
  const std::string text = "module calc\n"
                           "2;\n"
                           "2" +
                           operatorRun +
                           "\n"
                           "b%2;\n"
                           "b%2;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  const auto starsPos = text.find(operatorRun);
  ASSERT_NE(starsPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [&](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(starsPos) &&
               diagnostic.endOffset == static_cast<pegium::TextOffset>(
                                           starsPos + operatorRun.size());
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 4u) << parseDump;
}

TEST(RecoveryTest,
     IncompleteDiagnosticAfterInfixOperatorUsesFailureTokenAnchor) {
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
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "module name\n2   *", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_EQ(result.parseDiagnostics.size(), 1u) << parseDump;
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);
  EXPECT_EQ(result.parseDiagnostics.front().offset, 17u) << parseDump;
}

TEST(RecoveryTest,
     RepetitionRecoveryDoesNotStopOnRewoundFailureInsideActiveWindow) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
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
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
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

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_GE(parsedModule->statements.size(), 3u);
}

TEST(RecoveryTest,
     StarRepetitionDoesNotDisappearWhenFirstIterationAlreadyStarted) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  DataTypeRule<std::string> qualifiedName{"QualifiedName", some(id, "."_kw)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(qualifiedName)};
  ParserRule<RecoveryFeatureListNode> featureBlock{
      "FeatureBlock",
      "{"_kw + many(append<&RecoveryFeatureListNode::features>(feature)) +
          "}"_kw};

  const auto result = parseRule(featureBlock,
                                "{\n"
                                "  many comments Comment\n"
                                "  title: String\n"
                                "}\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedBlock =
      dynamic_cast<RecoveryFeatureListNode *>(result.value.get());
  ASSERT_NE(parsedBlock, nullptr);
  ASSERT_EQ(parsedBlock->features.size(), 2u) << parseDump;
  ASSERT_NE(parsedBlock->features[0], nullptr);
  ASSERT_NE(parsedBlock->features[1], nullptr);
  EXPECT_TRUE(parsedBlock->features[0]->many);
  EXPECT_EQ(parsedBlock->features[0]->name, "comments");
  EXPECT_EQ(parsedBlock->features[0]->type, "Comment") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->name, "title") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->type, "String") << parseDump;
}

TEST(
    RecoveryTest,
    BoundedNullableRepetitionDoesNotDisappearWhenFirstIterationAlreadyStarted) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  DataTypeRule<std::string> qualifiedName{"QualifiedName", some(id, "."_kw)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(qualifiedName)};
  ParserRule<RecoveryFeatureListNode> featureBlock{
      "FeatureBlock",
      "{"_kw +
          repeat<0, 3>(append<&RecoveryFeatureListNode::features>(feature)) +
          "}"_kw};

  const auto result = parseRule(featureBlock,
                                "{\n"
                                "  many comments Comment\n"
                                "  title: String\n"
                                "}\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedBlock =
      dynamic_cast<RecoveryFeatureListNode *>(result.value.get());
  ASSERT_NE(parsedBlock, nullptr);
  ASSERT_EQ(parsedBlock->features.size(), 2u) << parseDump;
  ASSERT_NE(parsedBlock->features[0], nullptr);
  ASSERT_NE(parsedBlock->features[1], nullptr);
  EXPECT_TRUE(parsedBlock->features[0]->many);
  EXPECT_EQ(parsedBlock->features[0]->name, "comments");
  EXPECT_EQ(parsedBlock->features[0]->type, "Comment") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->name, "title") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->type, "String") << parseDump;
}

TEST(
    RecoveryTest,
    NullableRepetitionCanDeleteGarbageBeforeRecoverableIterationAndRequiredSuffix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionNode> transition{
      "Transition", assign<&RecoveryTransitionNode::event>(id) + "=>"_kw +
                        assign<&RecoveryTransitionNode::target>(id)};
  ParserRule<RecoveryStateNode> state{
      "State", "state"_kw + assign<&RecoveryStateNode::name>(id) +
                   many(append<&RecoveryStateNode::transitions>(transition)) +
                   "end"_kw};

  const std::string text = "state Idle\n"
                           "<<<<<<<<<<<<<<<<<<<<<<<<\n"
                           "Start => Idle\n"
                           "end\n";
  const auto result = parseRule(state, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  const auto garbageBegin = text.find("<<<<<<<<<<<<<<<<<<<<<<<<");
  ASSERT_NE(garbageBegin, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [garbageBegin](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(garbageBegin);
      }))
      << parseDump;

  auto *parsedState = dynamic_cast<RecoveryStateNode *>(result.value.get());
  ASSERT_NE(parsedState, nullptr) << parseDump;
  EXPECT_EQ(parsedState->name, "Idle");
  ASSERT_EQ(parsedState->transitions.size(), 1u) << parseDump;
  ASSERT_NE(parsedState->transitions.front(), nullptr) << parseDump;
  EXPECT_EQ(parsedState->transitions.front()->event, "Start");
  EXPECT_EQ(parsedState->transitions.front()->target, "Idle");
}

TEST(RecoveryTest,
     RepetitionDeleteRetryDoesNotCrossStructuredSuffixAfterRecoveredIteration) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionNode> transition{
      "Transition", assign<&RecoveryTransitionNode::event>(id) + "=>"_kw +
                        assign<&RecoveryTransitionNode::target>(id)};
  ParserRule<RecoveryStateNode> state{
      "State", "state"_kw + assign<&RecoveryStateNode::name>(id) +
                   many(append<&RecoveryStateNode::transitions>(transition)) +
                   "end"_kw};
  ParserRule<RecoveryStateModelNode> model{
      "Model", some(append<&RecoveryStateModelNode::states>(state))};

  const std::string text = "state PowerOff\n"
                           "  switchCapacity > RedLight\n"
                           "end\n"
                           "\n"
                           "state RedLight\n"
                           "  switchCapacity => PowerOff\n"
                           "  next => GreenLight\n"
                           "end\n";
  const auto result = parseRule(model, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryStateModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->states.size(), 2u) << parseDump;
  ASSERT_NE(parsedModel->states[0], nullptr) << parseDump;
  ASSERT_NE(parsedModel->states[1], nullptr) << parseDump;
  EXPECT_EQ(parsedModel->states[0]->name, "PowerOff") << parseDump;
  EXPECT_EQ(parsedModel->states[1]->name, "RedLight") << parseDump;
  ASSERT_EQ(parsedModel->states[0]->transitions.size(), 1u) << parseDump;
  ASSERT_NE(parsedModel->states[0]->transitions.front(), nullptr) << parseDump;
  EXPECT_EQ(parsedModel->states[0]->transitions.front()->event,
            "switchCapacity");
  EXPECT_EQ(parsedModel->states[0]->transitions.front()->target, "RedLight");
}

TEST(RecoveryTest,
     FirstTopLevelElementCanRecoverLongGarbageLineUsingStableInternalPrefix) {
  struct RecoveryEntityNode : pegium::AstNode {
    string name;
    vector<pointer<RecoveryFeatureNode>> features;
  };

  struct RecoveryDomainModelNode : pegium::AstNode {
    vector<pointer<pegium::AstNode>> elements;
  };

  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(id)};
  ParserRule<RecoveryEntityNode> entity{
      "Entity", "entity"_kw + assign<&RecoveryEntityNode::name>(id) + "{"_kw +
                    many(append<&RecoveryEntityNode::features>(feature)) +
                    "}"_kw};
  ParserRule<RecoveryDomainModelNode> domainModel{
      "DomainModel", some(append<&RecoveryDomainModelNode::elements>(entity))};

  const std::string text = "entity Blog {\n"
                           "  <<<<<<<<<<<<<<<<<<<<<<<<\n"
                           "  title: String\n"
                           "}\n";
  const auto result = parseRule(domainModel, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Incomplete;
      }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryDomainModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->elements.size(), 1u) << parseDump;
  auto *blog =
      dynamic_cast<RecoveryEntityNode *>(parsedModel->elements.front().get());
  ASSERT_NE(blog, nullptr) << parseDump;
  EXPECT_EQ(blog->name, "Blog") << parseDump;
  ASSERT_EQ(blog->features.size(), 1u) << parseDump;
  ASSERT_NE(blog->features.front(), nullptr) << parseDump;
  EXPECT_EQ(blog->features.front()->name, "title") << parseDump;
  EXPECT_EQ(blog->features.front()->type, "String") << parseDump;
}

TEST(RecoveryTest,
     TopLevelChoicePrefersFullKeywordRepairOverShorterAlternativeRewrite) {
  struct RecoveryEntityNode : pegium::AstNode {
    string name;
  };

  struct RecoveryTypeNode : pegium::AstNode {
    string name;
  };

  struct RecoveryDomainModelNode : pegium::AstNode {
    vector<pointer<pegium::AstNode>> elements;
  };

  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTypeNode> dataType{
      "DataType", "datatype"_kw + assign<&RecoveryTypeNode::name>(id)};
  ParserRule<RecoveryEntityNode> entity{
      "Entity", "entity"_kw + assign<&RecoveryEntityNode::name>(id) + "{"_kw +
                    "}"_kw};
  ParserRule<pegium::AstNode> type{"Type", dataType | entity};
  ParserRule<RecoveryDomainModelNode> domainModel{
      "DomainModel", some(append<&RecoveryDomainModelNode::elements>(type))};

  const auto result = parseRule(domainModel, "entit Blog {}", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryDomainModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->elements.size(), 1u) << parseDump;
  auto *parsedEntity =
      dynamic_cast<RecoveryEntityNode *>(parsedModel->elements.front().get());
  ASSERT_NE(parsedEntity, nullptr) << parseDump;
  EXPECT_EQ(parsedEntity->name, "Blog");
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
                 assign<&RecoveryBinaryExpression::op>("+"_kw | "-"_kw |
                                                       "*"_kw) +
                 assign<&RecoveryBinaryExpression::right>(legacyPrimary))};
    Rule<RecoveryExpressionEvaluation> evaluation{
        "Evaluation", "legacy"_kw +
                          assign<&RecoveryExpressionEvaluation::expression>(
                              legacyExpression) +
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
