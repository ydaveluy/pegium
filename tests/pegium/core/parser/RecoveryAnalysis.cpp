#include <gtest/gtest.h>

#include <memory>

#include <pegium/core/RecoveryHarnessTestSupport.hpp>

using namespace pegium::parser;

namespace {

struct RecoveryAnalysisNode : pegium::AstNode {
  string name;
};

const auto kSimpleExpectedCst = R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 5,
          "grammarSource": "Literal",
          "text": "hello"
        },
        {
          "begin": 6,
          "end": 11,
          "grammarSource": "name=ID",
          "text": "world"
        }
      ],
      "end": 11,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json";

TEST(RecoveryFailureAnalysisTest, UsesContainingLeafWhenMaxCursorFallsInsideLeaf) {
  TerminalRule<> ws{"WS", some(s)};
  const auto hello = "hello"_kw;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryAnalysisNode> entry{
      "Entry", hello + assign<&RecoveryAnalysisNode::name>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto source =
      pegium::test::StrictRecoveryDocument(entry, "hello world", skipper);

  pegium::test::ExpectCst(*source.result.cst, kSimpleExpectedCst,
                          pegium::test::recovery_cst_json_options());

  pegium::parser::detail::FailureSnapshot snapshot;
  snapshot.maxCursorOffset = 7;
  pegium::parser::detail::FailureLeaf helloLeaf;
  helloLeaf.beginOffset = 0;
  helloLeaf.endOffset = 5;
  helloLeaf.element = std::addressof(hello);
  snapshot.failureLeafHistory.push_back(helloLeaf);
  pegium::parser::detail::FailureLeaf idLeaf;
  idLeaf.beginOffset = 6;
  idLeaf.endOffset = 11;
  idLeaf.element = std::addressof(id);
  snapshot.failureLeafHistory.push_back(idLeaf);
  pegium::parser::detail::finalize_failure_token_index(snapshot);

  EXPECT_TRUE(snapshot.hasFailureToken);
  EXPECT_EQ(snapshot.failureTokenIndex, 1u);
}

TEST(RecoveryFailureAnalysisTest,
     FallsBackToPreviousLeafWhenMaxCursorFallsBetweenVisibleLeaves) {
  TerminalRule<> ws{"WS", some(s)};
  const auto hello = "hello"_kw;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryAnalysisNode> entry{
      "Entry", hello + assign<&RecoveryAnalysisNode::name>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto harness =
      pegium::test::FailureAnalysisDocument(entry, "hello world !", skipper);

  pegium::test::ExpectCst(*harness.result.strictResult.cst, kSimpleExpectedCst,
                          pegium::test::recovery_cst_json_options());
  EXPECT_TRUE(harness.result.snapshot.hasFailureToken);
  EXPECT_EQ(harness.result.snapshot.failureTokenIndex, 1u);
}

TEST(RecoveryFailureAnalysisTest, HandlesInputWithoutVisibleLeaf) {
  ParserRule<RecoveryAnalysisNode> entry{
      "Entry", assign<&RecoveryAnalysisNode::name>("hello"_kw)};
  const auto harness =
      pegium::test::FailureAnalysisDocument(entry, "", NoOpSkipper());

  pegium::test::ExpectCst(
      *harness.result.strictResult.cst, R"json({
  "content": []
})json",
      pegium::test::recovery_cst_json_options());
  EXPECT_FALSE(harness.result.snapshot.hasFailureToken);
  EXPECT_TRUE(harness.result.snapshot.failureLeafHistory.empty());
}

TEST(RecoveryFailureAnalysisTest,
     CommittedCstSnapshotMatchesFailureAnalysisLeafHistory) {
  TerminalRule<> ws{"WS", some(s)};
  const auto hello = "hello"_kw;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryAnalysisNode> entry{
      "Entry", hello + assign<&RecoveryAnalysisNode::name>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto harness =
      pegium::test::FailureAnalysisDocument(entry, "hello world !", skipper);

  ASSERT_NE(harness.result.strictResult.cst, nullptr);
  const auto committedSnapshot = pegium::parser::detail::snapshot_from_committed_cst(
      *harness.result.strictResult.cst, harness.result.snapshot.maxCursorOffset);

  EXPECT_EQ(committedSnapshot.failureLeafHistory.size(),
            harness.result.snapshot.failureLeafHistory.size());
  EXPECT_EQ(committedSnapshot.hasFailureToken,
            harness.result.snapshot.hasFailureToken);
  EXPECT_EQ(committedSnapshot.failureTokenIndex,
            harness.result.snapshot.failureTokenIndex);
  EXPECT_EQ(committedSnapshot.maxCursorOffset,
            harness.result.snapshot.maxCursorOffset);
}

TEST(RecoveryWindowTest,
     ClampsInitialWindowToStrictParsedLengthInsideFirstConstruct) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  const auto field = id + ":"_kw + id;
  ParserRule<RecoveryAnalysisNode> entry{
      "Entry",
      "entity"_kw + assign<&RecoveryAnalysisNode::name>(id) + "{"_kw +
          many(field) + "}"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto harness = pegium::test::FailureAnalysisDocument(
      entry,
      "entity Blog {\n"
      "  title: String\n"
      "  content:: String\n"
      "}\n",
      skipper);

  EXPECT_FALSE(harness.result.strictResult.summary.entryRuleMatched);
  EXPECT_EQ(harness.result.strictResult.summary.parsedLength, 32u);
  EXPECT_GT(harness.result.strictResult.summary.maxCursorOffset,
            harness.result.strictResult.summary.parsedLength);
  const auto window = pegium::parser::detail::compute_recovery_window(
      harness.result.snapshot, 8u, 8u,
      harness.result.strictResult.summary.parsedLength);
  EXPECT_EQ(window.beginOffset, 0u);
  EXPECT_EQ(window.editFloorOffset,
            harness.result.strictResult.summary.parsedLength);
  EXPECT_TRUE(harness.result.snapshot.hasFailureToken);
}

TEST(RecoveryWindowTest, ComputesBackwardWindowStartFromFailureSnapshot) {
  TerminalRule<> ws{"WS", some(s)};
  const auto hello = "hello"_kw;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryAnalysisNode> entry{
      "Entry", hello + assign<&RecoveryAnalysisNode::name>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto source =
      pegium::test::StrictRecoveryDocument(entry, "hello world", skipper);

  pegium::test::ExpectCst(*source.result.cst, kSimpleExpectedCst,
                          pegium::test::recovery_cst_json_options());

  pegium::parser::detail::FailureSnapshot snapshot;
  snapshot.maxCursorOffset = 11;
  pegium::parser::detail::FailureLeaf helloLeaf;
  helloLeaf.beginOffset = 0;
  helloLeaf.endOffset = 5;
  helloLeaf.element = std::addressof(hello);
  snapshot.failureLeafHistory.push_back(helloLeaf);
  pegium::parser::detail::FailureLeaf idLeaf;
  idLeaf.beginOffset = 6;
  idLeaf.endOffset = 11;
  idLeaf.element = std::addressof(id);
  snapshot.failureLeafHistory.push_back(idLeaf);
  snapshot.failureTokenIndex = 1;
  snapshot.hasFailureToken = true;

  const auto window = pegium::parser::detail::compute_recovery_window(snapshot, 8);
  EXPECT_EQ(window.beginOffset, 0u);
  EXPECT_EQ(window.editFloorOffset, 0u);
  EXPECT_EQ(window.maxCursorOffset, 11u);
  EXPECT_EQ(window.visibleLeafBeginIndex, 0u);
}

TEST(RecoveryWindowTest, DoublesWindowTokenCountUntilConfiguredCap) {
  ParseOptions options;
  options.recoveryWindowTokenCount = 8;
  options.maxRecoveryWindowTokenCount = 32;

  EXPECT_EQ(pegium::parser::detail::next_recovery_window_token_count(8, options),
            std::optional<std::uint32_t>(16));
  EXPECT_EQ(pegium::parser::detail::next_recovery_window_token_count(16, options),
            std::optional<std::uint32_t>(32));
  EXPECT_EQ(pegium::parser::detail::next_recovery_window_token_count(32, options),
            std::nullopt);
}

TEST(RecoveryWindowTest, FallsBackToMaxCursorWhenNoVisibleLeafIsAvailable) {
  pegium::parser::detail::FailureSnapshot snapshot;
  snapshot.maxCursorOffset = 17;

  const auto window = pegium::parser::detail::compute_recovery_window(snapshot, 8);
  EXPECT_EQ(window.beginOffset, 17u);
  EXPECT_EQ(window.editFloorOffset, 17u);
  EXPECT_EQ(window.maxCursorOffset, 17u);
}

TEST(RecoveryWindowTest, CanWidenBackwardHistoryWithoutGrowingForwardStability) {
  pegium::parser::detail::FailureSnapshot snapshot;
  snapshot.maxCursorOffset = 17;
  snapshot.failureLeafHistory = {
      {.beginOffset = 0, .endOffset = 5, .element = nullptr},
      {.beginOffset = 6, .endOffset = 11, .element = nullptr},
      {.beginOffset = 12, .endOffset = 17, .element = nullptr},
  };
  snapshot.failureTokenIndex = 2;
  snapshot.hasFailureToken = true;

  const auto window =
      pegium::parser::detail::compute_recovery_window(snapshot, 16, 8, 0u);
  EXPECT_EQ(window.beginOffset, 0u);
  EXPECT_EQ(window.editFloorOffset, 0u);
  EXPECT_EQ(window.maxCursorOffset, 17u);
  EXPECT_EQ(window.tokenCount, 16u);
  EXPECT_EQ(window.forwardTokenCount, 8u);
}

TEST(RecoveryWindowTest, UsesVisibleLeafHistoryBeyondMaxCursorAsForwardLimit) {
  pegium::parser::detail::FailureSnapshot snapshot;
  snapshot.maxCursorOffset = 17;
  snapshot.failureLeafHistory = {
      {.beginOffset = 0, .endOffset = 5, .element = nullptr},
      {.beginOffset = 6, .endOffset = 11, .element = nullptr},
      {.beginOffset = 12, .endOffset = 17, .element = nullptr},
      {.beginOffset = 18, .endOffset = 23, .element = nullptr},
  };
  snapshot.failureTokenIndex = 2;
  snapshot.hasFailureToken = true;

  const auto window =
      pegium::parser::detail::compute_recovery_window(snapshot, 8, 8, 0u);
  EXPECT_EQ(window.beginOffset, 0u);
  EXPECT_EQ(window.editFloorOffset, 0u);
  EXPECT_EQ(window.maxCursorOffset, 23u);
}

} // namespace
