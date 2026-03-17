#include <gtest/gtest.h>

#include <pegium/RecoveryHarnessTestSupport.hpp>
#include <pegium/TestCstBuilderHarness.hpp>

using namespace pegium::parser;

namespace {

struct RecoveryHarnessNode : pegium::AstNode {
  string name;
};

TEST(RecoveryHarnessTest, ExposesStrictCstJson) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryHarnessNode> entry{"Entry",
                                        "hello"_kw + assign<&RecoveryHarnessNode::name>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const auto harness =
      pegium::test::StrictRecoveryDocument(entry, "hello world", skipper);
  ASSERT_NE(harness.result.cst, nullptr);

  pegium::test::ExpectCst(
      *harness.result.cst,
      R"json({
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
})json",
      pegium::test::recovery_cst_json_options());
  EXPECT_TRUE(harness.result.summary.entryRuleMatched);
  EXPECT_TRUE(harness.result.summary.fullMatch);
  EXPECT_EQ(harness.result.summary.parsedLength, 11u);
}

TEST(RecoveryHarnessTest, ExposesFailureAnalysisCstJson) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryHarnessNode> entry{"Entry",
                                        "hello"_kw + assign<&RecoveryHarnessNode::name>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const auto harness =
      pegium::test::FailureAnalysisDocument(entry, "hello world !", skipper);
  ASSERT_NE(harness.result.strictResult.cst, nullptr);

  pegium::test::ExpectCst(
      *harness.result.strictResult.cst,
      R"json({
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
})json",
      pegium::test::recovery_cst_json_options());
  EXPECT_TRUE(harness.result.snapshot.hasFailureToken);
  EXPECT_EQ(harness.result.snapshot.failureTokenIndex, 1u);
  EXPECT_EQ(harness.result.snapshot.maxCursorOffset, 12u);
}

TEST(RecoveryHarnessTest, ExposesDeleteScanAttemptCstJson) {
  const auto hello = "hello"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("xxhello");
  detail::FailureHistoryRecorder recorder(
      builderHarness.builder.input_begin());
  RecoveryContext ctx{builderHarness.builder, NoOpSkipper(), recorder};
  ASSERT_TRUE(parse(hello, ctx));
  pegium::test::ExpectCst(
      builderHarness.root,
      R"json({
  "content": [
    {
      "begin": 0,
      "end": 7,
      "grammarSource": "Literal",
      "recovered": true,
      "text": "xxhello"
    }
  ]
})json",
      pegium::test::recovery_cst_json_options());
}

} // namespace
