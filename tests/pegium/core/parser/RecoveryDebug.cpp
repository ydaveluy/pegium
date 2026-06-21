#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include <pegium/core/RecoveryHarnessTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/RecoveryDebug.hpp>
#include <pegium/core/parser/RecoverySearch.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

using namespace pegium::parser;

namespace {

detail::RecoveryAttemptSpec
build_attempt_spec(std::span<const detail::RecoveryWindow> /*selectedWindows*/,
                   const detail::RecoveryWindow &window) {
  return detail::RecoveryAttemptSpec{.window = window};
}

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

struct RecoveryDebugTripleNode : pegium::AstNode {
  string first;
  string second;
  string third;
};

struct RecoveryAttemptHarness {
  detail::RecoveryAttempt attempt;
  detail::RecoveryAttemptSpec spec;
};

template <typename RuleType>
RecoveryAttemptHarness
bestRecoveryAttempt(const RuleType &entryRule, std::string_view text,
                    const Skipper &skipper, ParseOptions options = {}) {
  RecoveryAttemptHarness harness;
  const auto snapshot = pegium::text::TextSnapshot::copy(text);

  const auto failureAnalysis = detail::run_strict_parse_with_failure_snapshot(
      entryRule, skipper, snapshot);
  const auto &strictSummary = failureAnalysis.strictResult.summary;
  const auto editFloorOffset =
      strictSummary.parsedLength != 0 ||
              !failureAnalysis.snapshot.hasFailureToken
          ? strictSummary.parsedLength
          : failureAnalysis.snapshot
                .failureLeafHistory[failureAnalysis.snapshot.failureTokenIndex]
                .beginOffset;
  const auto window = detail::compute_recovery_window(
      failureAnalysis.snapshot,
      std::max<std::uint32_t>(1u, options.recoveryWindowTokenCount),
      std::max<std::uint32_t>(1u, options.recoveryWindowTokenCount),
      editFloorOffset);
  const auto spec = build_attempt_spec({}, window);

  auto attempt =
      detail::execute_recovery_parse(entryRule, skipper, options, snapshot, spec);
  detail::classify_recovery_attempt(attempt);
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

TEST(RecoveryDebugTest,
     FailureSnapshotJsonOmitsFailureSiteWhenNoFailureTokenExists) {
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

TEST(RecoveryDebugTest,
     RecoveryAttemptJsonIncludesSpecDerivedSummariesAndOrderKey) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryDebugNode> entry{
      "Entry", assign<&RecoveryDebugNode::first>(id) + ";"_kw};

  const auto harness = bestRecoveryAttempt(entry, "hello", NoOpSkipper());
  const auto json =
      detail::recovery_attempt_to_json(harness.attempt, &harness.spec);
  const auto &object = json.object();

  EXPECT_EQ(object.at("status").string(), "Selectable");
  EXPECT_TRUE(object.at("entryRuleMatched").boolean());
  EXPECT_TRUE(object.at("fullMatch").boolean());
  EXPECT_GT(object.at("editCost").integer(), 0);
  EXPECT_EQ(object.at("editCount").integer(), 1);
  ASSERT_TRUE(object.at("recoveryEdits").isArray());
  ASSERT_EQ(object.at("recoveryEdits").array().size(), 1u);
  ASSERT_TRUE(object.at("parseDiagnostics").isArray());
  ASSERT_EQ(object.at("parseDiagnostics").array().size(), 1u);
  EXPECT_FALSE(object.at("parseDiagnostics")
                   .array()
                   .front()
                   .object()
                   .contains("anchor"));

  ASSERT_TRUE(object.at("editTrace").isObject());
  const auto &editTrace = object.at("editTrace").object();
  EXPECT_EQ(editTrace.at("insertCount").integer(), 1);
  EXPECT_EQ(editTrace.at("entryCount").integer(), 1);
  EXPECT_TRUE(editTrace.at("hasEdits").boolean());

  // `score` was a pre-shared-order projection of fields already exposed at
  // top level / in editTrace; assert those instead.
  EXPECT_EQ(object.at("status").string(), "Selectable");
  EXPECT_TRUE(object.at("fullMatch").boolean());
  EXPECT_GT(object.at("editCost").integer(), 0);

  ASSERT_TRUE(object.at("orderKey").isObject());
  const auto &orderKey = object.at("orderKey").object();
  // The dump mirrors every is_better_recovery_key axis (raw fields + the two
  // derived scores the comparator orders on), so a dump fully explains a rank.
  EXPECT_TRUE(orderKey.at("fullMatch").boolean());
  EXPECT_TRUE(orderKey.at("matched").boolean());
  EXPECT_GE(orderKey.at("firstEditOffset").integer(), 0);
  EXPECT_GE(orderKey.at("firstEditScore").integer(), 0);
  EXPECT_GT(orderKey.at("editCost").integer(), 0);
  EXPECT_GT(orderKey.at("editCount").integer(), 0);
  EXPECT_GT(orderKey.at("faithfulness").integer(), 0);
  EXPECT_GE(orderKey.at("progressAfterEdits").integer(), 0);

  ASSERT_TRUE(object.at("spec").isObject());
  const auto &spec = object.at("spec").object();
  ASSERT_TRUE(spec.at("window").isObject());
  const auto &window = spec.at("window").object();
  EXPECT_EQ(window.at("backwardTokenCount").integer(), 16);
  EXPECT_EQ(window.at("forwardTokenCount").integer(), 16);
  ASSERT_TRUE(object.at("replayWindow").isObject());
  const auto &replayWindow = object.at("replayWindow").object();
  EXPECT_EQ(replayWindow.at("beginOffset").integer(), 0);
  EXPECT_EQ(replayWindow.at("maxCursorOffset").integer(), 5);
}

TEST(RecoveryDebugTest,
     RecoverySearchRunJsonSummarizesPipelineCountersAndSelectedWindows) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryDebugNode> entry{
      "Entry", assign<&RecoveryDebugNode::first>(id) + ";"_kw};

  ParseOptions options;
  options.recoveryWindowTokenCount = 4;
  const auto text = pegium::text::TextSnapshot::copy("hello");
  const auto search =
      detail::orchestrate_recovery_search(entry, NoOpSkipper(), options, text);
  const auto json = detail::recovery_search_run_to_json(search);
  const auto &object = json.object();

  EXPECT_EQ(object.at("strictParseRuns").integer(), 1);
  EXPECT_GE(object.at("recoveryWindowsTried").integer(), 1);
  EXPECT_GE(object.at("recoveryAttemptRuns").integer(), 1);
  EXPECT_EQ(object.at("failureVisibleCursorOffset").integer(),
            search.failureVisibleCursorOffset);

  ASSERT_TRUE(object.at("selectedWindows").isArray());
  EXPECT_EQ(object.at("selectedWindows").array().size(),
            search.selectedWindows.size());

  ASSERT_TRUE(object.at("selectedAttempt").isObject());
  const auto &selectedAttempt = object.at("selectedAttempt").object();
  EXPECT_EQ(selectedAttempt.at("parsedLength").integer(),
            search.selectedAttempt.parsedLength);
  EXPECT_EQ(selectedAttempt.at("status").string(), "Selectable");
  EXPECT_EQ(selectedAttempt.contains("replayWindow"),
            search.selectedAttempt.replayWindow.has_value());
}

TEST(RecoveryDebugTest,
     RecoverySearchRunJsonSummarizesFallbackSelectionWhenNoCredibleWindowWins) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryDebugTripleNode> entry{
      "Entry", assign<&RecoveryDebugTripleNode::first>(id) + ";"_kw +
                   assign<&RecoveryDebugTripleNode::second>(id) + ";"_kw +
                   assign<&RecoveryDebugTripleNode::third>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  ParseOptions options;
  options.recoveryWindowTokenCount = 2;
  options.maxRecoveryEditsPerAttempt = 1;

  const auto text = pegium::text::TextSnapshot::copy("one two three");
  const auto search =
      detail::orchestrate_recovery_search(entry, skipper, options, text);
  const auto json = detail::recovery_search_run_to_json(search);
  const auto &object = json.object();

  EXPECT_EQ(object.at("strictParseRuns").integer(), 1);
  EXPECT_GE(object.at("recoveryWindowsTried").integer(), 1);
  EXPECT_GE(object.at("recoveryAttemptRuns").integer(), 1);
  ASSERT_TRUE(object.at("selectedWindows").isArray());
  EXPECT_FALSE(object.at("selectedWindows").array().empty());

  ASSERT_TRUE(object.at("selectedAttempt").isObject());
  const auto &selectedAttempt = object.at("selectedAttempt").object();
  EXPECT_TRUE(selectedAttempt.at("entryRuleMatched").boolean());
  EXPECT_TRUE(selectedAttempt.at("fullMatch").boolean());
  EXPECT_EQ(selectedAttempt.at("status").string(), "Selectable");
}

TEST(RecoveryDebugTest, RecoveryWindowsJsonUsesSymmetricTokenCounts) {
  const std::vector<detail::RecoveryWindow> windows{
      {.beginOffset = 0,
       .editFloorOffset = 0,
       .maxCursorOffset = 4,
       .tokenCount = 1,
       .forwardTokenCount = 1,
       .visibleLeafBeginIndex = 0},
      {.beginOffset = 6,
       .editFloorOffset = 8,
       .maxCursorOffset = 11,
       .tokenCount = 2,
       .forwardTokenCount = 2,
       .visibleLeafBeginIndex = 1},
  };

  const auto json = detail::recovery_windows_to_json(windows);
  ASSERT_TRUE(json.isArray());
  ASSERT_EQ(json.array().size(), 2u);

  const auto &first = json.array()[0].object();
  EXPECT_EQ(first.at("beginOffset").integer(), 0);
  EXPECT_EQ(first.at("editFloorOffset").integer(), 0);
  EXPECT_EQ(first.at("maxCursorOffset").integer(), 4);
  EXPECT_EQ(first.at("backwardTokenCount").integer(), 1);
  EXPECT_EQ(first.at("forwardTokenCount").integer(), 1);

  const auto &second = json.array()[1].object();
  EXPECT_EQ(second.at("beginOffset").integer(), 6);
  EXPECT_EQ(second.at("editFloorOffset").integer(), 8);
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

  const auto harness = bestRecoveryAttempt(definition, "def a :;", skipper, {});
  const auto json =
      detail::recovery_attempt_to_json(harness.attempt, &harness.spec);
  const auto &parseDiagnostics = json.object().at("parseDiagnostics").array();

  ASSERT_FALSE(parseDiagnostics.empty());
  ASSERT_TRUE(parseDiagnostics.front().object().at("element").isObject());
  const auto &element =
      parseDiagnostics.front().object().at("element").object();
  EXPECT_EQ(
      element.at("kind").integer(),
      static_cast<std::int64_t>(pegium::grammar::ElementKind::Assignment));
  EXPECT_NE(element.at("text").string().find("Expression"), std::string::npos);
}

} // namespace
