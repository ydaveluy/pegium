#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <pegium/RecoveryHarnessTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/parser/RecoveryDebug.hpp>
#include <pegium/parser/RecoverySearch.hpp>

using namespace pegium::parser;

namespace {

struct RecoveryDebugNode : pegium::AstNode {
  string first;
};

struct RecoveryDebugExpression : pegium::AstNode {};

struct RecoveryDebugNumberExpression : RecoveryDebugExpression {
  int value = 0;
};

struct RecoveryDebugBinaryExpression : RecoveryDebugExpression {
  pointer<RecoveryDebugExpression> left;
  string op;
  pointer<RecoveryDebugExpression> right;
};

struct RecoveryDebugDefinition : pegium::AstNode {
  string name;
  pointer<RecoveryDebugExpression> expr;
};

struct RecoveryAttemptHarness {
  std::unique_ptr<pegium::workspace::Document> document;
  detail::RecoveryAttempt attempt;
  detail::RecoveryAttemptSpec spec;
};

template <typename RuleType>
RecoveryAttemptHarness bestRecoveryAttempt(const RuleType &entryRule,
                                           std::string_view text,
                                           const Skipper &skipper,
                                           ParseOptions options = {}) {
  RecoveryAttemptHarness harness;
  harness.document = std::make_unique<pegium::workspace::Document>();
  harness.document->setText(std::string{text});

  const auto strictResult =
      detail::run_strict_parse(entryRule, skipper, *harness.document);
  const auto failureAnalysis = detail::analyze_failure(
      entryRule, skipper, *harness.document, strictResult.summary);
  const auto window = detail::compute_recovery_window(
      failureAnalysis.snapshot,
      std::max<std::uint32_t>(1u, options.recoveryWindowTokenCount));
  const auto spec = detail::build_recovery_attempt_spec({}, window);

  auto attempt = detail::run_recovery_attempt(entryRule, skipper, options,
                                              *harness.document, spec);
  detail::classify_recovery_attempt(attempt);
  detail::score_recovery_attempt(attempt);
  if (!harness.attempt.cst ||
      detail::is_better_recovery_attempt(attempt, harness.attempt)) {
    harness.spec = spec;
    harness.attempt = std::move(attempt);
  }
  return harness;
}

TEST(RecoveryDebugTest, FailureSnapshotJsonIncludesFailureTokenAndLeafHistory) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryDebugNode> entry{
      "Entry", "hello"_kw + assign<&RecoveryDebugNode::first>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const auto harness =
      pegium::test::FailureAnalysisDocument(entry, "hello world !", skipper);
  const auto json = detail::failure_snapshot_to_json(harness.result.snapshot);
  const auto &object = json.object();

  EXPECT_EQ(object.at("maxCursorOffset").integer(), 12);
  EXPECT_TRUE(object.at("hasFailureToken").boolean());
  EXPECT_EQ(object.at("failureTokenIndex").integer(), 1);
  ASSERT_TRUE(object.at("failureLeafHistory").isArray());
  EXPECT_EQ(object.at("failureLeafHistory").array().size(), 2u);
  ASSERT_TRUE(object.at("failureToken").isObject());
  const auto &failureToken = object.at("failureToken").object();
  EXPECT_EQ(failureToken.at("beginOffset").integer(), 6);
  EXPECT_EQ(failureToken.at("endOffset").integer(), 11);
  EXPECT_FALSE(object.contains("failureSite"));
}

TEST(RecoveryDebugTest, FailureSnapshotJsonOmitsFailureSiteWhenNoFailureTokenExists) {
  ParserRule<RecoveryDebugNode> entry{
      "Entry", assign<&RecoveryDebugNode::first>("service"_kw)};

  const auto harness =
      pegium::test::FailureAnalysisDocument(entry, "", NoOpSkipper());
  const auto json = detail::failure_snapshot_to_json(harness.result.snapshot);
  const auto &object = json.object();

  EXPECT_FALSE(object.at("hasFailureToken").boolean());
  EXPECT_TRUE(object.at("failureToken").isNull());
  EXPECT_FALSE(object.contains("failureSite"));
}

TEST(RecoveryDebugTest, RecoveryAttemptJsonIncludesSpecEditTraceAndScore) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryDebugNode> entry{
      "Entry", assign<&RecoveryDebugNode::first>(id) + ";"_kw};

  const auto harness = bestRecoveryAttempt(entry, "hello", NoOpSkipper());
  const auto json =
      detail::recovery_attempt_to_json(harness.attempt, &harness.spec);
  const auto &object = json.object();

  EXPECT_EQ(object.at("status").string(), "Stable");
  EXPECT_TRUE(object.at("entryRuleMatched").boolean());
  EXPECT_TRUE(object.at("fullMatch").boolean());
  EXPECT_GT(object.at("editCost").integer(), 0);
  EXPECT_EQ(object.at("editCount").integer(), 1);
  ASSERT_TRUE(object.at("parseDiagnostics").isArray());
  ASSERT_EQ(object.at("parseDiagnostics").array().size(), 1u);
  EXPECT_FALSE(object.at("parseDiagnostics").array().front().object().contains(
      "anchor"));

  ASSERT_TRUE(object.at("editTrace").isObject());
  const auto &editTrace = object.at("editTrace").object();
  EXPECT_EQ(editTrace.at("insertCount").integer(), 1);
  EXPECT_EQ(editTrace.at("diagnosticCount").integer(), 1);
  EXPECT_TRUE(editTrace.at("hasEdits").boolean());

  ASSERT_TRUE(object.at("score").isObject());
  const auto &score = object.at("score").object();
  EXPECT_TRUE(score.at("stable").boolean());
  EXPECT_TRUE(score.at("credible").boolean());
  EXPECT_TRUE(score.at("fullMatch").boolean());
  EXPECT_GT(score.at("editCost").integer(), 0);

  ASSERT_TRUE(object.at("spec").isObject());
  const auto &spec = object.at("spec").object();
  ASSERT_TRUE(spec.at("windows").isArray());
  ASSERT_EQ(spec.at("windows").array().size(), 1u);
  const auto &window = spec.at("windows").array().front().object();
  EXPECT_EQ(window.at("backwardTokenCount").integer(), 8);
  EXPECT_EQ(window.at("forwardTokenCount").integer(), 8);
}

TEST(RecoveryDebugTest, RecoveryWindowsJsonUsesSymmetricTokenCounts) {
  const std::vector<detail::RecoveryWindow> windows{
      {.beginOffset = 0, .maxCursorOffset = 4, .tokenCount = 1, .visibleLeafBeginIndex = 0},
      {.beginOffset = 6, .maxCursorOffset = 11, .tokenCount = 2, .visibleLeafBeginIndex = 1},
  };

  const auto json = detail::recovery_windows_to_json(windows);
  ASSERT_TRUE(json.isArray());
  ASSERT_EQ(json.array().size(), 2u);

  const auto &first = json.array()[0].object();
  EXPECT_EQ(first.at("beginOffset").integer(), 0);
  EXPECT_EQ(first.at("maxCursorOffset").integer(), 4);
  EXPECT_EQ(first.at("backwardTokenCount").integer(), 1);
  EXPECT_EQ(first.at("forwardTokenCount").integer(), 1);

  const auto &second = json.array()[1].object();
  EXPECT_EQ(second.at("beginOffset").integer(), 6);
  EXPECT_EQ(second.at("maxCursorOffset").integer(), 11);
  EXPECT_EQ(second.at("backwardTokenCount").integer(), 2);
  EXPECT_EQ(second.at("forwardTokenCount").integer(), 2);
  EXPECT_EQ(second.at("visibleLeafBeginIndex").integer(), 1);
}

TEST(RecoveryDebugTest,
     RecoveryAttemptJsonSummarizesRecursiveGrammarElementsWithoutPrintingThem) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryDebugExpression> primary{
      "Primary", create<RecoveryDebugNumberExpression>() +
                     assign<&RecoveryDebugNumberExpression::value>(number)};
  InfixRule<RecoveryDebugBinaryExpression, &RecoveryDebugBinaryExpression::left,
            &RecoveryDebugBinaryExpression::op,
            &RecoveryDebugBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryDebugExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryDebugDefinition> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDebugDefinition::name>(id) + ":"_kw +
          assign<&RecoveryDebugDefinition::expr>(expressionRule) + ";"_kw};

  const auto harness =
      bestRecoveryAttempt(definition, "def a :;", skipper, {});
  const auto json =
      detail::recovery_attempt_to_json(harness.attempt, &harness.spec);
  const auto &parseDiagnostics = json.object().at("parseDiagnostics").array();

  ASSERT_FALSE(parseDiagnostics.empty());
  ASSERT_TRUE(parseDiagnostics.front().object().at("element").isObject());
  const auto &element = parseDiagnostics.front().object().at("element").object();
  EXPECT_EQ(element.at("kind").integer(),
            static_cast<std::int64_t>(pegium::grammar::ElementKind::Assignment));
  EXPECT_NE(element.at("text").string().find("Expression"), std::string::npos);
}

} // namespace
