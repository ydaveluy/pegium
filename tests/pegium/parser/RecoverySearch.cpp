#include <gtest/gtest.h>

#include <pegium/ParseJsonTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/RecoverySearch.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

using namespace pegium::parser;

namespace {

constexpr pegium::converter::CstJsonConversionOptions kRecoveryCstJsonOptions{
    .includeText = true,
    .includeGrammarSource = true,
    .includeHidden = false,
    .includeRecovered = true,
};

struct RecoverySearchNode : pegium::AstNode {
  string name;
};

struct RecoveryPairNode : pegium::AstNode {
  string first;
  string second;
};

struct RecoveryTripleNode : pegium::AstNode {
  string first;
  string second;
  string third;
};

struct RecoveryQuadNode : pegium::AstNode {
  string first;
  string second;
  string third;
  string fourth;
};

struct RecoveryQuintNode : pegium::AstNode {
  string first;
  string second;
  string third;
  string fourth;
  string fifth;
};

struct RecoveryStatementNode : pegium::AstNode {
  string name;
};

struct RecoveryStatementListNode : pegium::AstNode {
  vector<pointer<pegium::AstNode>> statements;
};

struct RecoveryAttemptHarness {
  detail::RecoveryAttempt attempt;
};

template <typename RuleType>
RecoveryAttemptHarness bestRecoveryAttempt(const RuleType &entryRule,
                                           std::string_view text,
                                           const Skipper &skipper,
  ParseOptions options = {}) {
  RecoveryAttemptHarness harness;
  const auto snapshot = pegium::text::TextSnapshot::copy(text);

  const auto strictResult = detail::run_strict_parse(entryRule, skipper, snapshot);
  const auto failureAnalysis = detail::analyze_failure(
      entryRule, skipper, snapshot, strictResult.summary);
  const auto window = detail::compute_recovery_window(
      failureAnalysis.snapshot,
      std::max<std::uint32_t>(1u, options.recoveryWindowTokenCount));
  const auto spec = detail::build_recovery_attempt_spec({}, window);
  auto attempt =
      detail::run_recovery_attempt(entryRule, skipper, options, snapshot, spec);
  detail::classify_recovery_attempt(attempt);
  detail::score_recovery_attempt(attempt);
  if (!harness.attempt.cst ||
      detail::is_better_recovery_attempt(attempt, harness.attempt)) {
    harness.attempt = std::move(attempt);
  }
  return harness;
}

TEST(RecoverySearchTest, MissingDelimiterUsesInsertionAttempt) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>(id) + ";"_kw};

  ParseOptions options;
  options.recoveryWindowTokenCount = 4;
  options.maxRecoveryWindowTokenCount = 8;
  const auto result = pegium::test::Parse(entry, "hello", options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 5,
          "grammarSource": "name=ID",
          "text": "hello"
        }
      ],
      "end": 5,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json");
  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Inserted);
}

TEST(RecoverySearchTest,
     StatementChoicePrefersInsertedDelimiterOverDeletingCurrentStatement) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryStatementNode> tagged{
      "Tagged", "def"_kw + assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<RecoveryStatementNode> plain{
      "Plain", assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", tagged | plain};
  ParserRule<RecoveryStatementListNode> entry{
      "Entry",
      some(append<&RecoveryStatementListNode::statements>(statement))};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const auto parsed = pegium::test::Parse(entry, "def a\ndef b;", skipper);

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
    return diag.kind == ParseDiagnosticKind::Inserted;
  }));
  EXPECT_FALSE(std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
    return diag.kind == ParseDiagnosticKind::Deleted;
  }));

  auto *root = pegium::ast_ptr_cast<RecoveryStatementListNode>(parsed.value);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->statements.size(), 2u);
  auto *first =
      dynamic_cast<RecoveryStatementNode *>(root->statements[0].get());
  auto *second =
      dynamic_cast<RecoveryStatementNode *>(root->statements[1].get());
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->name, "a");
  EXPECT_EQ(second->name, "b");
}

TEST(RecoverySearchTest,
     StatementChoicePrefersDeletingStrayTokenOverShortLateInsertion) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryStatementNode> tagged{
      "Tagged", "def"_kw + assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<RecoveryStatementNode> plain{
      "Plain", assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", tagged | plain};
  ParserRule<RecoveryStatementListNode> entry{
      "Entry",
      some(append<&RecoveryStatementListNode::statements>(statement))};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const std::string input =
      "def a;\n"
      "v\n"
      "def b;\n";
  const auto parsed = pegium::test::Parse(entry, input, skipper);

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto strayPos = input.find("\nv\n");
  ASSERT_NE(strayPos, std::string::npos);
  const auto strayOffset = static_cast<pegium::TextOffset>(strayPos + 1);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [&](const auto &diag) {
    return diag.kind == ParseDiagnosticKind::Deleted &&
           diag.offset == strayOffset;
  }));

  auto *root = pegium::ast_ptr_cast<RecoveryStatementListNode>(parsed.value);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->statements.size(), 2u);
  auto *first =
      dynamic_cast<RecoveryStatementNode *>(root->statements[0].get());
  auto *second =
      dynamic_cast<RecoveryStatementNode *>(root->statements[1].get());
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->name, "a");
  EXPECT_EQ(second->name, "b");
}

TEST(RecoverySearchTest, RecoveryAttemptSpecsStayGenericWithoutAnchorVariants) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryPairNode> entry{
      "Entry", assign<&RecoveryPairNode::first>(id) + ";"_kw +
                   assign<&RecoveryPairNode::second>(id)};
  const auto text = pegium::text::TextSnapshot::copy("one two");

  const auto strictResult =
      detail::run_strict_parse(entry, NoOpSkipper(), text);
  const auto failureAnalysis = detail::analyze_failure(
      entry, NoOpSkipper(), text, strictResult.summary);
  const auto window =
      detail::compute_recovery_window(failureAnalysis.snapshot, 2u);
  const auto spec = detail::build_recovery_attempt_spec({}, window);

  ASSERT_EQ(spec.windows.size(), 1u);
  EXPECT_EQ(spec.windows.front().beginOffset, window.beginOffset);
  EXPECT_EQ(spec.windows.front().maxCursorOffset,
            window.maxCursorOffset);
}

TEST(RecoverySearchTest, RecoveryReportStaysEmptyWhenStrictParseMatches) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>(id)};

  const auto result = pegium::test::Parse(entry, "hello");

  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(result.parseDiagnostics.empty());
  EXPECT_FALSE(result.recoveryReport.hasRecovered);
  EXPECT_FALSE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 0u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 0u);
  EXPECT_FALSE(result.recoveryReport.lastRecoveryWindow.has_value());
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingPrefersFartherParseBeforeEditPosition) {
  detail::RecoveryAttempt farther;
  farther.score = {
      .entryRuleMatched = true,
      .stable = true,
      .credible = true,
      .editCost = 1,
      .fullMatch = false,
      .editSpan = 0,
      .diagnosticCount = 1,
      .firstEditOffset = 10,
      .parsedLength = 24,
      .maxCursorOffset = 24,
  };

  detail::RecoveryAttempt nearer;
  nearer.score = {
      .entryRuleMatched = true,
      .stable = true,
      .credible = true,
      .editCost = 1,
      .fullMatch = false,
      .editSpan = 0,
      .diagnosticCount = 1,
      .firstEditOffset = 18,
      .parsedLength = 20,
      .maxCursorOffset = 20,
  };

  EXPECT_TRUE(detail::is_better_recovery_attempt(farther, nearer));
  EXPECT_FALSE(detail::is_better_recovery_attempt(nearer, farther));
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingPrefersLaterEditWhenAttemptsOtherwiseTie) {
  detail::RecoveryAttempt earlier;
  earlier.score = {
      .entryRuleMatched = true,
      .stable = true,
      .credible = true,
      .editCost = 1,
      .fullMatch = true,
      .editSpan = 0,
      .diagnosticCount = 1,
      .firstEditOffset = 8,
      .parsedLength = 24,
      .maxCursorOffset = 24,
  };

  detail::RecoveryAttempt later;
  later.score = {
      .entryRuleMatched = true,
      .stable = true,
      .credible = true,
      .editCost = 1,
      .fullMatch = true,
      .editSpan = 0,
      .diagnosticCount = 1,
      .firstEditOffset = 12,
      .parsedLength = 24,
      .maxCursorOffset = 24,
  };

  EXPECT_TRUE(detail::is_better_recovery_attempt(later, earlier));
  EXPECT_FALSE(detail::is_better_recovery_attempt(earlier, later));
}

TEST(RecoverySearchTest, WordLiteralSingleSubstitutionCanRecoverGenerically) {
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>("service"_kw)};

  const auto result = pegium::test::Parse(entry, "servixe");

  EXPECT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
}

TEST(RecoverySearchTest, LongWordLiteralSingleSubstitutionCanRecoverGenerically) {
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>("catalogue"_kw)};

  const auto result = pegium::test::Parse(entry, "catalogoe");
  EXPECT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
}

TEST(RecoverySearchTest, ParserKeepsStrictResultWhenNoSelectableAttemptExists) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTripleNode> entry{
      "Entry", assign<&RecoveryTripleNode::first>(id) + ";"_kw +
                   assign<&RecoveryTripleNode::second>(id) + ";"_kw +
                   assign<&RecoveryTripleNode::third>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 2;
  options.maxRecoveryWindowTokenCount = 2;
  options.maxRecoveryWindows = 1;
  options.maxRecoveryEditsPerAttempt = 1;

  const auto result =
      pegium::test::Parse(entry, "one two three", skipper, options);
  const auto text = pegium::text::TextSnapshot::copy("one two three");
  const auto strictResult =
      detail::run_strict_parse(entry, skipper, text);
  const auto failureAnalysis = detail::analyze_failure(
      entry, skipper, text, strictResult.summary);
  const auto window = detail::compute_recovery_window(
      failureAnalysis.snapshot, options.recoveryWindowTokenCount);
  const auto spec = detail::build_recovery_attempt_spec({}, window);
  bool foundSelectableAttempt = false;
  auto attempt =
      detail::run_recovery_attempt(entry, skipper, options, text, spec);
  detail::classify_recovery_attempt(attempt);
  foundSelectableAttempt |= detail::is_selectable_recovery_attempt(attempt);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": []
})json",
      kRecoveryCstJsonOptions);
  EXPECT_FALSE(result.fullMatch);
  EXPECT_FALSE(foundSelectableAttempt);
}

TEST(RecoverySearchTest, EditBudgetCanDisableRecoveryAttempts) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>(id) + "{"_kw + "}"_kw};

  ParseOptions options;
  options.maxRecoveryEditsPerAttempt = 0;
  const auto result =
      pegium::test::Parse(entry, "hello{", NoOpSkipper(), options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": []
})json");
  EXPECT_FALSE(result.fullMatch);
}

TEST(RecoverySearchTest, SingleWindowResumesStrictParsingAfterRecovery) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryPairNode> entry{
      "Entry", assign<&RecoveryPairNode::first>(id) + ";"_kw +
                   assign<&RecoveryPairNode::second>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 1;
  options.maxRecoveryWindowTokenCount = 1;
  options.maxRecoveryWindows = 1;
  const auto result = pegium::test::Parse(entry, "one two", skipper, options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 3,
          "grammarSource": "first=ID",
          "text": "one"
        },
        {
          "begin": 4,
          "end": 7,
          "grammarSource": "second=ID",
          "text": "two"
        }
      ],
      "end": 7,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json");
  EXPECT_TRUE(result.fullMatch);
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Inserted);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_TRUE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 1u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->beginOffset, 0u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->maxCursorOffset, 4u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->backwardTokenCount, 1u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->forwardTokenCount, 1u);
}

TEST(RecoverySearchTest, ParserCanRecoverSeparatedErrorsAcrossTwoWindows) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryQuadNode> entry{
      "Entry", assign<&RecoveryQuadNode::first>(id) + ";"_kw +
                   assign<&RecoveryQuadNode::second>(id) + ";"_kw +
                   assign<&RecoveryQuadNode::third>(id) + ";"_kw +
                   assign<&RecoveryQuadNode::fourth>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 1;
  options.maxRecoveryWindowTokenCount = 1;
  options.maxRecoveryWindows = 2;
  const auto result =
      pegium::test::Parse(entry, "one;two three four", skipper, options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 3,
          "grammarSource": "first=ID",
          "text": "one"
        },
        {
          "begin": 3,
          "end": 4,
          "grammarSource": "Literal",
          "text": ";"
        },
        {
          "begin": 4,
          "end": 7,
          "grammarSource": "second=ID",
          "text": "two"
        },
        {
          "begin": 8,
          "end": 13,
          "grammarSource": "third=ID",
          "text": "three"
        },
        {
          "begin": 14,
          "end": 18,
          "grammarSource": "fourth=ID",
          "text": "four"
        }
      ],
      "end": 18,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json",
      kRecoveryCstJsonOptions);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_EQ(result.parseDiagnostics.size(), 2u);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_TRUE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 2u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 2u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->backwardTokenCount, 1u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->forwardTokenCount, 1u);
  EXPECT_GE(result.recoveryReport.lastRecoveryWindow->beginOffset, 3u);
  EXPECT_GE(result.recoveryReport.lastRecoveryWindow->maxCursorOffset,
            result.recoveryReport.lastRecoveryWindow->beginOffset);
}

TEST(RecoverySearchTest, ParserCanRecoverSeparatedErrorsAcrossTwoWindowsWithTrivia) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryQuadNode> entry{
      "Entry", assign<&RecoveryQuadNode::first>(id) + ";"_kw +
                   assign<&RecoveryQuadNode::second>(id) + ";"_kw +
                   assign<&RecoveryQuadNode::third>(id) + ";"_kw +
                   assign<&RecoveryQuadNode::fourth>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 1;
  options.maxRecoveryWindowTokenCount = 1;
  options.maxRecoveryWindows = 2;

  const auto result = pegium::test::Parse(
      entry, "one;\n\ntwo three\n\nfour", skipper, options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 3,
          "grammarSource": "first=ID",
          "text": "one"
        },
        {
          "begin": 3,
          "end": 4,
          "grammarSource": "Literal",
          "text": ";"
        },
        {
          "begin": 6,
          "end": 9,
          "grammarSource": "second=ID",
          "text": "two"
        },
        {
          "begin": 10,
          "end": 15,
          "grammarSource": "third=ID",
          "text": "three"
        },
        {
          "begin": 17,
          "end": 21,
          "grammarSource": "fourth=ID",
          "text": "four"
        }
      ],
      "end": 21,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json",
      kRecoveryCstJsonOptions);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_EQ(result.parseDiagnostics.size(), 2u);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_TRUE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 2u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 2u);
}

TEST(RecoverySearchTest,
     ParserRepairsSupportedMultiEditWordLiteralAndContinues) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryPairNode> entry{
      "Entry", "catalogue"_kw + assign<&RecoveryPairNode::first>(id) + ";"_kw +
                   assign<&RecoveryPairNode::second>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 1;
  options.maxRecoveryWindowTokenCount = 1;
  options.maxRecoveryWindows = 2;

  const auto result =
      pegium::test::Parse(entry, "catalogxoe demo next", skipper, options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 10,
          "grammarSource": "Literal",
          "recovered": true,
          "text": "catalogxoe"
        },
        {
          "begin": 11,
          "end": 15,
          "grammarSource": "first=ID",
          "text": "demo"
        },
        {
          "begin": 16,
          "end": 20,
          "grammarSource": "second=ID",
          "text": "next"
        }
      ],
      "end": 20,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json",
      kRecoveryCstJsonOptions);
  EXPECT_TRUE(result.fullMatch);
  ASSERT_EQ(result.parseDiagnostics.size(), 2u);
  ASSERT_TRUE(result.value);
}

TEST(RecoverySearchTest,
     ParserRejectsWordLiteralWhenFuzzyScoreStaysTooPoorInFirstWindow) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryPairNode> entry{
      "Entry", "catalogue"_kw + assign<&RecoveryPairNode::first>(id) + ";"_kw +
                   assign<&RecoveryPairNode::second>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 1;
  options.maxRecoveryWindowTokenCount = 1;
  options.maxRecoveryWindows = 2;

  const auto result =
      pegium::test::Parse(entry, "cxtxlgxxe demo next", skipper, options);

  pegium::test::ExpectCst(result, R"json({
  "content": []
})json",
                          kRecoveryCstJsonOptions);
  EXPECT_FALSE(result.fullMatch);
  EXPECT_FALSE(result.value);
  ASSERT_FALSE(result.parseDiagnostics.empty());
}

TEST(RecoverySearchTest, ParserSingleWindowStopsAtNextSeparatedError) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryQuintNode> entry{
      "Entry", assign<&RecoveryQuintNode::first>(id) + ";"_kw +
                   assign<&RecoveryQuintNode::second>(id) + ";"_kw +
                   assign<&RecoveryQuintNode::third>(id) + ";"_kw +
                   assign<&RecoveryQuintNode::fourth>(id) + ";"_kw +
                   assign<&RecoveryQuintNode::fifth>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  ParseOptions options;
  options.recoveryWindowTokenCount = 1;
  options.maxRecoveryWindowTokenCount = 1;
  options.maxRecoveryWindows = 1;
  const auto result =
      pegium::test::Parse(entry, "one;two three;four five", skipper, options);

  pegium::test::ExpectCst(
      result,
      R"json({
  "content": [
    {
      "begin": 0,
      "content": [
        {
          "begin": 0,
          "end": 3,
          "grammarSource": "first=ID",
          "text": "one"
        },
        {
          "begin": 3,
          "end": 4,
          "grammarSource": "Literal",
          "text": ";"
        },
        {
          "begin": 4,
          "end": 7,
          "grammarSource": "second=ID",
          "text": "two"
        },
        {
          "begin": 8,
          "end": 13,
          "grammarSource": "third=ID",
          "text": "three"
        },
        {
          "begin": 13,
          "end": 14,
          "grammarSource": "Literal",
          "text": ";"
        },
        {
          "begin": 14,
          "end": 18,
          "grammarSource": "fourth=ID",
          "text": "four"
        }
      ],
      "end": 18,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json",
      kRecoveryCstJsonOptions);
  EXPECT_FALSE(result.fullMatch);
  EXPECT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_FALSE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 1u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
}

} // namespace
