#include <gtest/gtest.h>

#include <array>

#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/RecoveryDebug.hpp>
#include <pegium/core/parser/RecoverySearch.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

using namespace pegium::parser;

namespace {

detail::StrictParseResult
run_strict_parse(const auto &entryRule, const Skipper &skipper,
                 const pegium::text::TextSnapshot &text,
                 const pegium::utils::CancellationToken &cancelToken = {}) {
  const detail::StrictFailureEngine strictFailureEngine;
  return strictFailureEngine.runStrictParse(entryRule, skipper, text,
                                            cancelToken);
}

detail::StrictFailureEngineResult
inspect_failure(const auto &entryRule, const Skipper &skipper,
                const pegium::text::TextSnapshot &text,
                const detail::StrictParseSummary &summary,
                const pegium::utils::CancellationToken &cancelToken = {}) {
  const detail::StrictFailureEngine strictFailureEngine;
  return strictFailureEngine.inspectFailure(entryRule, skipper, text, summary,
                                            cancelToken);
}

detail::RecoveryAttemptSpec
build_attempt_spec(std::span<const detail::RecoveryWindow> selectedWindows,
                   const detail::RecoveryWindow &window) {
  detail::WindowPlanner planner{ParseOptions{}};
  planner.seedAcceptedWindows(selectedWindows);
  return planner.buildAttemptSpec(window);
}

void set_recovery_edits(
    detail::RecoveryAttempt &attempt,
    std::initializer_list<ParseDiagnostic> diagnostics) {
  attempt.recoveryEdits.clear();
  attempt.recoveryEdits.reserve(diagnostics.size());
  for (const auto &diagnostic : diagnostics) {
    attempt.recoveryEdits.push_back({.kind = diagnostic.kind,
                                     .offset = diagnostic.offset,
                                     .beginOffset = diagnostic.beginOffset,
                                     .endOffset = diagnostic.endOffset,
                                     .element = diagnostic.element,
                                     .message = diagnostic.message});
  }
}

std::unique_ptr<pegium::RootCstNode>
make_attempt_cst(std::string_view text,
                 std::initializer_list<
                     std::pair<pegium::TextOffset, pegium::TextOffset>>
                     visibleLeafSpans) {
  auto cst = std::make_unique<pegium::RootCstNode>(
      pegium::text::TextSnapshot::copy(std::string(text)));
  pegium::CstBuilder builder(*cst);
  static const auto leafElement = "leaf"_kw;
  for (const auto &[beginOffset, endOffset] : visibleLeafSpans) {
    builder.leaf(beginOffset, endOffset, std::addressof(leafElement));
  }
  return cst;
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
  }
  return dump;
}

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

struct RecoverySearchTransitionNode : pegium::AstNode {
  string event;
  string target;
};

struct RecoverySearchStateNode : pegium::AstNode {
  string name;
  vector<pointer<RecoverySearchTransitionNode>> transitions;
};

struct RecoveryFeatureNode : pegium::AstNode {
  bool many = false;
  string name;
  string type;
};

struct RecoveryEntityNode : pegium::AstNode {
  string name;
  vector<pointer<RecoveryFeatureNode>> features;
};

struct RecoveryDataTypeNode : pegium::AstNode {
  string name;
};

struct RecoveryTypeListNode : pegium::AstNode {
  vector<pointer<pegium::AstNode>> elements;
};

struct RecoveryAttemptHarness {
  detail::RecoveryAttempt attempt;
};

template <typename RuleType>
RecoveryAttemptHarness
bestRecoveryAttempt(const RuleType &entryRule, std::string_view text,
                    const Skipper &skipper, ParseOptions options = {}) {
  RecoveryAttemptHarness harness;
  const auto snapshot = pegium::text::TextSnapshot::copy(text);

  const auto strictResult = run_strict_parse(entryRule, skipper, snapshot);
  const auto failureAnalysis =
      inspect_failure(entryRule, skipper, snapshot, strictResult.summary);
  const auto editFloorOffset =
      strictResult.summary.parsedLength != 0 ||
              !failureAnalysis.snapshot.hasFailureToken
          ? strictResult.summary.parsedLength
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
      detail::run_recovery_attempt(entryRule, skipper, options, snapshot, spec);
  detail::classify_recovery_attempt(attempt);
  detail::score_recovery_attempt(attempt);
  if (!harness.attempt.cst ||
      detail::is_better_recovery_attempt(attempt, harness.attempt)) {
    harness.attempt = std::move(attempt);
  }
  return harness;
}

std::string summarize_statement_names(const RecoveryStatementListNode &root) {
  std::string summary;
  for (const auto &statement : root.statements) {
    const auto *node =
        dynamic_cast<const RecoveryStatementNode *>(statement.get());
    if (node == nullptr) {
      continue;
    }
    if (!summary.empty()) {
      summary += "|";
    }
    summary += node->name;
  }
  return summary;
}

TEST(RecoverySearchTest, MissingDelimiterUsesInsertionAttempt) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>(id) + ";"_kw};

  ParseOptions options;
  options.recoveryWindowTokenCount = 4;
  options.maxRecoveryWindowTokenCount = 8;
  const auto result = pegium::test::Parse(entry, "hello", options);

  pegium::test::ExpectCst(result,
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
        },
        {
          "begin": 5,
          "end": 5,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
        }
      ],
      "end": 5,
      "grammarSource": "Rule(Entry)"
    }
  ]
})json");
  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Inserted);
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
      "Entry", some(append<&RecoveryStatementListNode::statements>(statement))};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const auto parsed = pegium::test::Parse(entry, "def a\ndef b;", skipper);

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(
      std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Inserted;
      }));
  EXPECT_FALSE(
      std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
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

TEST(
    RecoverySearchTest,
    StatementChoiceKeepsFollowingStatementRecoverableAfterStrayStandaloneToken) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryStatementNode> tagged{
      "Tagged", "def"_kw + assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<RecoveryStatementNode> plain{
      "Plain", assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", tagged | plain};
  ParserRule<RecoveryStatementListNode> entry{
      "Entry", some(append<&RecoveryStatementListNode::statements>(statement))};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const std::string input = "def a;\n"
                            "v\n"
                            "def b;\n";
  const auto parsed = pegium::test::Parse(entry, input, skipper);

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto strayPos = input.find("\nv\n");
  ASSERT_NE(strayPos, std::string::npos);
  const auto strayOffset = static_cast<pegium::TextOffset>(strayPos + 1);
  const auto nextStatementPos = input.find("def b;");
  ASSERT_NE(nextStatementPos, std::string::npos);
  const auto nextStatementOffset =
      static_cast<pegium::TextOffset>(nextStatementPos);
  EXPECT_TRUE(
      std::ranges::any_of(parsed.parseDiagnostics, [&](const auto &diag) {
        if (!diag.isSyntax()) {
          return false;
        }
        return diag.beginOffset >= strayOffset &&
               diag.beginOffset <= nextStatementOffset;
      }));

  auto *root = pegium::ast_ptr_cast<RecoveryStatementListNode>(parsed.value);
  ASSERT_NE(root, nullptr);
  ASSERT_GE(root->statements.size(), 2u);
  auto *first =
      dynamic_cast<RecoveryStatementNode *>(root->statements[0].get());
  auto *last =
      dynamic_cast<RecoveryStatementNode *>(root->statements.back().get());
  ASSERT_NE(first, nullptr);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(first->name, "a");
  EXPECT_EQ(last->name, "b");
}

TEST(
    RecoverySearchTest,
    InitialRecoveryWindowDoesNotRewriteEarlierTopLevelChoiceBeforeStrictFrontier) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(id)};
  ParserRule<RecoveryDataTypeNode> dataType{
      "DataType", "datatype"_kw + assign<&RecoveryDataTypeNode::name>(id)};
  ParserRule<RecoveryEntityNode> entity{
      "Entity", "entity"_kw + assign<&RecoveryEntityNode::name>(id) + "{"_kw +
                    many(append<&RecoveryEntityNode::features>(feature)) +
                    "}"_kw};
  ParserRule<pegium::AstNode> type{"Type", dataType | entity};
  ParserRule<RecoveryTypeListNode> entry{
      "Entry", some(append<&RecoveryTypeListNode::elements>(type))};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  const auto parsed = pegium::test::Parse(entry,
                                          "entity Post {\n"
                                          "  many comments Comment\n"
                                          "  title: String\n"
                                          "}\n"
                                          "\n"
                                          "entity Comment {}\n",
                                          skipper);

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_FALSE(
      std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Replaced &&
               diag.beginOffset == 0u;
      }));

  auto *root = pegium::ast_ptr_cast<RecoveryTypeListNode>(parsed.value);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->elements.size(), 2u);
  auto *post = dynamic_cast<RecoveryEntityNode *>(root->elements[0].get());
  auto *comment = dynamic_cast<RecoveryEntityNode *>(root->elements[1].get());
  ASSERT_NE(post, nullptr);
  ASSERT_NE(comment, nullptr);
  EXPECT_EQ(post->name, "Post");
  EXPECT_EQ(comment->name, "Comment");
  ASSERT_EQ(post->features.size(), 2u);
  ASSERT_NE(post->features[0], nullptr);
  EXPECT_TRUE(post->features[0]->many);
  EXPECT_EQ(post->features[0]->name, "comments");
  EXPECT_EQ(post->features[0]->type, "Comment");
}

TEST(RecoverySearchTest, RecoveryAttemptSpecsStayGenericWithoutAnchorVariants) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryPairNode> entry{
      "Entry", assign<&RecoveryPairNode::first>(id) + ";"_kw +
                   assign<&RecoveryPairNode::second>(id)};
  const auto text = pegium::text::TextSnapshot::copy("one two");

  const auto strictResult = run_strict_parse(entry, NoOpSkipper(), text);
  const auto failureAnalysis =
      inspect_failure(entry, NoOpSkipper(), text, strictResult.summary);
  const auto window =
      detail::compute_recovery_window(failureAnalysis.snapshot, 2u);
  const auto spec = build_attempt_spec({}, window);

  ASSERT_EQ(spec.windows.size(), 1u);
  EXPECT_EQ(spec.windows.front().beginOffset, window.beginOffset);
  EXPECT_EQ(spec.windows.front().maxCursorOffset, window.maxCursorOffset);
}

TEST(RecoverySearchTest,
     WindowPlannerAcceptKeepsValidatedFollowUpReplayContract) {
  detail::WindowPlanner planner{ParseOptions{}};
  const detail::RecoveryWindow first{
      .beginOffset = 10u,
      .editFloorOffset = 14u,
      .maxCursorOffset = 20u,
      .forwardTokenCount = 8u,
      .visibleLeafBeginIndex = 3u,
      .stablePrefixOffset = 14u,
      .hasStablePrefix = true,
  };
  const detail::RecoveryWindow second{
      .beginOffset = 18u,
      .editFloorOffset = 24u,
      .maxCursorOffset = 30u,
      .forwardTokenCount = 8u,
      .visibleLeafBeginIndex = 7u,
      .stablePrefixOffset = 24u,
      .hasStablePrefix = true,
  };
  const detail::RecoveryWindow third{
      .beginOffset = 28u,
      .editFloorOffset = 36u,
      .maxCursorOffset = 44u,
      .forwardTokenCount = 16u,
      .visibleLeafBeginIndex = 11u,
      .stablePrefixOffset = 36u,
      .hasStablePrefix = true,
  };
  planner.seedAcceptedWindows(std::array{first});

  detail::RecoveryAttempt acceptedAttempt;
  acceptedAttempt.replayWindows = {first, second, third};

  planner.accept(acceptedAttempt, second);

  ASSERT_EQ(planner.acceptedWindows().size(), 3u);
  EXPECT_EQ(planner.acceptedWindows()[0].beginOffset, first.beginOffset);
  EXPECT_EQ(planner.acceptedWindows()[1].beginOffset, second.beginOffset);
  EXPECT_EQ(planner.acceptedWindows()[2].beginOffset, third.beginOffset);
  EXPECT_EQ(planner.acceptedWindows()[2].editFloorOffset,
            third.editFloorOffset);
}

TEST(
    RecoverySearchTest,
    ExplicitRecoveryAttemptWindowCanDeleteGarbageBeforeRecoverableIterationAndSuffix) {
  const auto skipper = SkipperBuilder().ignore(some(s)).build();
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchTransitionNode> transition{
      "Transition", assign<&RecoverySearchTransitionNode::event>(id) + "=>"_kw +
                        assign<&RecoverySearchTransitionNode::target>(id)};
  ParserRule<RecoverySearchStateNode> entry{
      "State",
      "state"_kw + assign<&RecoverySearchStateNode::name>(id) +
          many(append<&RecoverySearchStateNode::transitions>(transition)) +
          "end"_kw};

  const auto text =
      pegium::text::TextSnapshot::copy("state Idle\n"
                                       "<<<<<<<<<<<<<<<<<<<<<<<<\n"
                                       "Start => Idle\n"
                                       "end\n");
  ParseOptions options;
  options.recoveryWindowTokenCount = 8;
  options.maxRecoveryWindowTokenCount = 8;
  const detail::RecoveryWindow window{
      .beginOffset = 0,
      .editFloorOffset = 0,
      .maxCursorOffset = 11,
      .tokenCount = 8,
      .forwardTokenCount = 8,
  };
  const auto spec = build_attempt_spec({}, window);
  auto attempt =
      detail::run_recovery_attempt(entry, skipper, options, text, spec);
  detail::classify_recovery_attempt(attempt);
  detail::score_recovery_attempt(attempt);
  const auto diagnostics =
      detail::materialize_syntax_diagnostics(attempt.recoveryEdits);

  EXPECT_TRUE(attempt.entryRuleMatched);
  EXPECT_TRUE(attempt.fullMatch);
  EXPECT_EQ(attempt.parsedLength, static_cast<pegium::TextOffset>(text.size()));
  EXPECT_TRUE(
      std::ranges::any_of(diagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Deleted;
      }));
  EXPECT_NE(attempt.status, detail::RecoveryAttemptStatus::StrictFailure);
}

TEST(RecoverySearchTest,
     ExplicitStablePrefixAttemptCanDeleteGarbageFeatureLineInsideEntity) {
  const auto skipper = SkipperBuilder().ignore(some(s)).build();
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(id)};
  ParserRule<RecoveryEntityNode> entry{
      "Entity", "entity"_kw + assign<&RecoveryEntityNode::name>(id) + "{"_kw +
                    many(append<&RecoveryEntityNode::features>(feature)) +
                    "}"_kw};

  const auto text =
      pegium::text::TextSnapshot::copy("entity Blog {\n"
                                       "  <<<<<<<<<<<<<<<<<<<<<<<<\n"
                                       "  title: String\n"
                                       "}\n");
  ParseOptions options;
  options.recoveryWindowTokenCount = 8;
  options.maxRecoveryWindowTokenCount = 8;
  const detail::RecoveryWindow window{
      .beginOffset = 13,
      .editFloorOffset = 13,
      .maxCursorOffset = 13,
      .tokenCount = 8,
      .forwardTokenCount = 8,
      .stablePrefixOffset = 13,
      .hasStablePrefix = true,
  };
  const auto spec = build_attempt_spec({}, window);
  auto attempt =
      detail::run_recovery_attempt(entry, skipper, options, text, spec);
  detail::classify_recovery_attempt(attempt);
  detail::score_recovery_attempt(attempt);
  const auto diagnostics =
      detail::materialize_syntax_diagnostics(attempt.recoveryEdits);

  EXPECT_TRUE(attempt.entryRuleMatched);
  EXPECT_TRUE(attempt.fullMatch);
  EXPECT_EQ(attempt.parsedLength, static_cast<pegium::TextOffset>(text.size()));
  EXPECT_TRUE(
      std::ranges::any_of(diagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Deleted;
      }));
  EXPECT_TRUE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     RecoveryAttemptSpecPreservesSelectedWindowReplayContract) {
  const detail::RecoveryWindow selectedWindow{
      .beginOffset = 4,
      .editFloorOffset = 9,
      .maxCursorOffset = 18,
      .tokenCount = 3,
      .forwardTokenCount = 6,
      .visibleLeafBeginIndex = 2,
      .stablePrefixOffset = 9,
      .hasStablePrefix = true,
  };
  const detail::RecoveryWindow laterWindow{
      .beginOffset = 24,
      .editFloorOffset = 27,
      .maxCursorOffset = 31,
      .tokenCount = 2,
      .forwardTokenCount = 4,
      .visibleLeafBeginIndex = 7,
      .stablePrefixOffset = 0,
      .hasStablePrefix = false,
  };
  const std::array selectedWindows{selectedWindow};

  const auto spec = build_attempt_spec(selectedWindows, laterWindow);

  ASSERT_EQ(spec.windows.size(), 2u);
  EXPECT_EQ(spec.windows.front().beginOffset, selectedWindow.beginOffset);
  EXPECT_EQ(spec.windows.front().editFloorOffset,
            selectedWindow.editFloorOffset);
  EXPECT_EQ(spec.windows.front().maxCursorOffset,
            selectedWindow.maxCursorOffset);
  EXPECT_EQ(spec.windows.front().tokenCount, selectedWindow.tokenCount);
  EXPECT_EQ(spec.windows.front().forwardTokenCount,
            selectedWindow.forwardTokenCount);
  EXPECT_EQ(spec.windows.front().visibleLeafBeginIndex,
            selectedWindow.visibleLeafBeginIndex);
  EXPECT_EQ(spec.windows.front().stablePrefixOffset,
            selectedWindow.stablePrefixOffset);
  EXPECT_EQ(spec.windows.front().hasStablePrefix,
            selectedWindow.hasStablePrefix);
  EXPECT_EQ(spec.windows.back().editFloorOffset, laterWindow.editFloorOffset);
  EXPECT_EQ(spec.windows.back().forwardTokenCount,
            laterWindow.forwardTokenCount);
  EXPECT_EQ(spec.windows.back().stablePrefixOffset,
            laterWindow.stablePrefixOffset);
  EXPECT_EQ(spec.windows.back().hasStablePrefix, laterWindow.hasStablePrefix);
}

TEST(RecoverySearchTest,
     FallbackReplayCarriesParsedPrefixIntoNextWindowFloor) {
  detail::WindowPlanner planner{ParseOptions{}};
  const detail::RecoveryWindow acceptedWindow{
      .beginOffset = 0,
      .editFloorOffset = 4,
      .maxCursorOffset = 4,
      .tokenCount = 2,
      .forwardTokenCount = 0,
      .visibleLeafBeginIndex = 0,
      .stablePrefixOffset = 4,
      .hasStablePrefix = true,
  };
  planner.seedAcceptedWindows(std::span<const detail::RecoveryWindow>{
      &acceptedWindow, 1u});

  detail::FailureSnapshot snapshot{
      .maxCursorOffset = 14,
      .failureLeafHistory =
          {
              {.beginOffset = 0, .endOffset = 3, .element = nullptr},
              {.beginOffset = 4, .endOffset = 7, .element = nullptr},
              {.beginOffset = 8, .endOffset = 12, .element = nullptr},
              {.beginOffset = 13, .endOffset = 15, .element = nullptr},
          },
      .failureTokenIndex = 3,
      .hasFailureToken = true,
  };
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.parsedLength = 12;

  planner.begin(snapshot, selectedAttempt);
  const auto planned = planner.plan();

  EXPECT_EQ(planned.window.editFloorOffset, 12);
  EXPECT_EQ(planned.window.stablePrefixOffset, 12);
  EXPECT_TRUE(planned.window.hasStablePrefix);
}

TEST(RecoverySearchTest, FallbackAcceptancePreservesReplayForwardTokenCount) {
  detail::WindowPlanner planner{ParseOptions{}};
  detail::RecoveryAttempt attempt;
  attempt.replayWindows.push_back({
      .beginOffset = 0,
      .editFloorOffset = 4,
      .maxCursorOffset = 9,
      .tokenCount = 2,
      .forwardTokenCount = 6,
      .visibleLeafBeginIndex = 0,
      .stablePrefixOffset = 4,
      .hasStablePrefix = true,
  });

  const detail::RecoveryWindow plannedWindow{
      .beginOffset = 0,
      .editFloorOffset = 4,
      .maxCursorOffset = 9,
      .tokenCount = 2,
      .forwardTokenCount = 2,
      .visibleLeafBeginIndex = 0,
      .stablePrefixOffset = 4,
      .hasStablePrefix = true,
  };

  planner.accept(attempt, plannedWindow);
  ASSERT_EQ(planner.acceptedWindows().size(), 1u);
  EXPECT_EQ(planner.acceptedWindows().front().forwardTokenCount, 6u);
}

TEST(RecoverySearchTest,
     StablePrefixWindowsDoNotFallbackToFullHistoryReplay) {
  ParseOptions options;
  options.recoveryWindowTokenCount = 2;
  options.maxRecoveryWindowTokenCount = 64;
  detail::WindowPlanner planner{options};

  detail::FailureSnapshot snapshot{
      .maxCursorOffset = 23,
      .failureLeafHistory =
          {
              {.beginOffset = 0, .endOffset = 3, .element = nullptr},
              {.beginOffset = 4, .endOffset = 7, .element = nullptr},
              {.beginOffset = 8, .endOffset = 11, .element = nullptr},
              {.beginOffset = 12, .endOffset = 15, .element = nullptr},
              {.beginOffset = 16, .endOffset = 19, .element = nullptr},
              {.beginOffset = 20, .endOffset = 23, .element = nullptr},
          },
      .failureTokenIndex = 5,
      .hasFailureToken = true,
  };
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.parsedLength = 19;

  planner.begin(snapshot, selectedAttempt);
  const auto planned = planner.plan();

  EXPECT_GT(planned.window.beginOffset, 0);
  EXPECT_TRUE(planned.window.hasStablePrefix);
  EXPECT_TRUE(planner.advance(planned.window));

  const auto widened = planner.plan();
  EXPECT_EQ(widened.window.beginOffset, planned.window.beginOffset);
  EXPECT_EQ(widened.window.editFloorOffset, planned.window.editFloorOffset);
  EXPECT_EQ(widened.window.tokenCount, planned.window.tokenCount);
  EXPECT_EQ(widened.window.forwardTokenCount, 4u);
}

TEST(RecoverySearchTest,
     StablePrefixWindowsCanWidenBackwardWithoutReopeningEditFloor) {
  ParseOptions options;
  options.recoveryWindowTokenCount = 2;
  options.maxRecoveryWindowTokenCount = 4;
  detail::WindowPlanner planner{options};

  detail::FailureSnapshot snapshot{
      .maxCursorOffset = 23,
      .failureLeafHistory =
          {
              {.beginOffset = 0, .endOffset = 3, .element = nullptr},
              {.beginOffset = 4, .endOffset = 7, .element = nullptr},
              {.beginOffset = 8, .endOffset = 11, .element = nullptr},
              {.beginOffset = 12, .endOffset = 15, .element = nullptr},
              {.beginOffset = 16, .endOffset = 19, .element = nullptr},
              {.beginOffset = 20, .endOffset = 23, .element = nullptr},
          },
      .failureTokenIndex = 5,
      .hasFailureToken = true,
  };
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.parsedLength = 19;

  planner.begin(snapshot, selectedAttempt);
  const auto planned = planner.plan();
  ASSERT_TRUE(planner.advance(planned.window));

  const auto forwardWidened = planner.plan();
  ASSERT_EQ(forwardWidened.window.beginOffset, planned.window.beginOffset);
  ASSERT_EQ(forwardWidened.window.editFloorOffset, planned.window.editFloorOffset);
  ASSERT_EQ(forwardWidened.window.forwardTokenCount, 4u);

  ASSERT_TRUE(planner.advance(forwardWidened.window, true));
  const auto backwardWidened = planner.plan();
  EXPECT_LT(backwardWidened.window.beginOffset, forwardWidened.window.beginOffset);
  EXPECT_EQ(backwardWidened.window.editFloorOffset,
            forwardWidened.window.editFloorOffset);
  EXPECT_EQ(backwardWidened.window.forwardTokenCount,
            forwardWidened.window.forwardTokenCount);
}

TEST(RecoverySearchTest,
     BudgetOverflowFullMatchAtStablePrefixBoundaryRemainsSelectable) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.stablePrefixOffset = 10;
  attempt.hasStablePrefix = true;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 112;
  attempt.fullMatch = true;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 10,
                                .beginOffset = 10,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status, detail::RecoveryAttemptStatus::Stable);
  EXPECT_TRUE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     BudgetOverflowAcrossLaterReplayWindowIsNotSelectable) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.stablePrefixOffset = 22;
  attempt.hasStablePrefix = true;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 107;
  attempt.parsedLength = 113;
  attempt.maxCursorOffset = 113;
  attempt.fullMatch = true;
  attempt.stableAfterRecovery = true;
  attempt.reachedRecoveryTarget = true;
  attempt.replayWindows = {
      {.beginOffset = 3,
       .editFloorOffset = 22,
       .maxCursorOffset = 32,
       .tokenCount = 8,
       .forwardTokenCount = 8,
       .visibleLeafBeginIndex = 0,
       .stablePrefixOffset = 22,
       .hasStablePrefix = true},
      {.beginOffset = 10,
       .editFloorOffset = 39,
       .maxCursorOffset = 75,
       .tokenCount = 8,
       .forwardTokenCount = 8,
       .visibleLeafBeginIndex = 1,
       .stablePrefixOffset = 39,
       .hasStablePrefix = true},
  };
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 32,
                                .beginOffset = 32,
                                .endOffset = 32,
                                .element = nullptr,
                                .message = {}},
                               {.kind = ParseDiagnosticKind::Deleted,
                                .offset = 73,
                                .beginOffset = 73,
                                .endOffset = 94,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     BudgetOverflowFullMatchWithoutStablePrefixAtEditFloorIsNotSelectable) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.stablePrefixOffset = 0;
  attempt.hasStablePrefix = false;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 112;
  attempt.fullMatch = true;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 10,
                                .beginOffset = 10,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     BudgetOverflowFullMatchDeletingInputPrefixWithoutStablePrefixRemainsSelectable) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = true;
  attempt.parsedLength = 53;
  attempt.lastVisibleCursorOffset = 53;
  attempt.maxCursorOffset = 53;
  attempt.stablePrefixOffset = 0;
  attempt.hasStablePrefix = false;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 112;
  attempt.replayWindows = {{.beginOffset = 0,
                            .editFloorOffset = 0,
                            .maxCursorOffset = 0,
                            .tokenCount = 1,
                            .forwardTokenCount = 1,
                            .visibleLeafBeginIndex = 0,
                            .stablePrefixOffset = 0,
                            .hasStablePrefix = false}};
  attempt.cst = make_attempt_cst("req login", {{24u, 27u}, {28u, 33u}});
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 0,
                                .beginOffset = 0,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status, detail::RecoveryAttemptStatus::Stable);
  EXPECT_TRUE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     LaterRecoveryWindowDoesNotDegradeEarlierRecoveredStatementPrefix) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryStatementNode> statement{
      "Statement",
      "def"_kw + assign<&RecoveryStatementNode::name>(id) + ";"_kw};
  ParserRule<RecoveryStatementListNode> entry{
      "Entry", some(append<&RecoveryStatementListNode::statements>(statement))};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  ParseOptions options;
  options.recoveryWindowTokenCount = 4;
  options.maxRecoveryWindowTokenCount = 16;

  const std::string baselineInput = "def a\n"
                                    "def b\n"
                                    "\n"
                                    "def c;\n"
                                    "def d;\n"
                                    "\n"
                                    "def e;\n";
  const std::string laterErrorInput = "def a\n"
                                      "def b\n"
                                      "\n"
                                      "def c;\n"
                                      "def d;\n"
                                      "\n"
                                      "xx\n"
                                      "\n"
                                      "def e;\n";

  const auto baseline =
      pegium::test::Parse(entry, baselineInput, skipper, options);
  const auto laterError =
      pegium::test::Parse(entry, laterErrorInput, skipper, options);
  const auto baselineDump = dump_parse_diagnostics(baseline.parseDiagnostics);
  const auto laterErrorDump =
      dump_parse_diagnostics(laterError.parseDiagnostics);

  ASSERT_TRUE(baseline.value) << baselineDump;
  ASSERT_TRUE(laterError.value) << laterErrorDump;
  ASSERT_TRUE(baseline.fullMatch) << baselineDump;
  ASSERT_TRUE(laterError.fullMatch) << laterErrorDump;
  EXPECT_TRUE(baseline.recoveryReport.hasRecovered);
  EXPECT_TRUE(laterError.recoveryReport.hasRecovered);
  EXPECT_GE(laterError.recoveryReport.recoveryCount, 2u);

  auto *baselineRoot =
      pegium::ast_ptr_cast<RecoveryStatementListNode>(baseline.value);
  auto *laterErrorRoot =
      pegium::ast_ptr_cast<RecoveryStatementListNode>(laterError.value);
  ASSERT_NE(baselineRoot, nullptr);
  ASSERT_NE(laterErrorRoot, nullptr);

  EXPECT_EQ(summarize_statement_names(*baselineRoot), "a|b|c|d|e");
  const auto laterErrorSummary = summarize_statement_names(*laterErrorRoot);
  EXPECT_TRUE(std::string_view(laterErrorSummary).starts_with("a|b|c|d"))
      << laterErrorSummary;

  const auto laterErrorMarker = laterErrorInput.find("\nxx\n");
  ASSERT_NE(laterErrorMarker, std::string::npos);
  const auto laterErrorOffset =
      static_cast<pegium::TextOffset>(laterErrorMarker + 1u);
  EXPECT_EQ(std::ranges::count_if(laterError.parseDiagnostics,
                                  [&](const auto &diag) {
                                    return diag.kind ==
                                               ParseDiagnosticKind::Inserted &&
                                           diag.offset < laterErrorOffset;
                                  }),
            2u);
  EXPECT_FALSE(
      std::ranges::any_of(laterError.parseDiagnostics, [&](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Deleted &&
               diag.beginOffset < laterErrorOffset;
      }));
}

TEST(RecoverySearchTest, RecoveryReportStaysEmptyWhenStrictParseMatches) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{"Entry",
                                       assign<&RecoverySearchNode::name>(id)};

  const auto result = pegium::test::Parse(entry, "hello");

  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(result.parseDiagnostics.empty());
  EXPECT_FALSE(result.recoveryReport.hasRecovered);
  EXPECT_FALSE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 0u);
  EXPECT_EQ(result.recoveryReport.recoveryAttemptRuns, 0u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 0u);
  EXPECT_FALSE(result.recoveryReport.lastRecoveryWindow.has_value());
}

TEST(RecoverySearchTest, ParserResultMatchesGlobalRecoverySearchRunResult) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>(id) + ";"_kw};

  ParseOptions options;
  options.recoveryWindowTokenCount = 4;
  options.maxRecoveryWindowTokenCount = 8;
  const auto text = pegium::text::TextSnapshot::copy("hello");

  const auto search =
      detail::run_recovery_search(entry, NoOpSkipper(), options, text);
  const auto parsed =
      pegium::test::Parse(entry, "hello", NoOpSkipper(), options);

  EXPECT_EQ(parsed.fullMatch, search.selectedAttempt.fullMatch);
  EXPECT_EQ(parsed.parsedLength, search.selectedAttempt.parsedLength);
  EXPECT_EQ(parsed.lastVisibleCursorOffset,
            search.selectedAttempt.lastVisibleCursorOffset);
  EXPECT_EQ(parsed.failureVisibleCursorOffset,
            search.failureVisibleCursorOffset);
  EXPECT_EQ(parsed.maxCursorOffset, search.selectedAttempt.maxCursorOffset);

  EXPECT_EQ(parsed.recoveryReport.hasRecovered,
            !search.selectedWindows.empty());
  EXPECT_EQ(parsed.recoveryReport.fullRecovered,
            !search.selectedWindows.empty() &&
                search.selectedAttempt.fullMatch);
  EXPECT_EQ(parsed.recoveryReport.recoveryCount,
            static_cast<std::uint32_t>(search.selectedWindows.size()));
  EXPECT_EQ(parsed.recoveryReport.recoveryWindowsTried,
            search.recoveryWindowsTried);
  EXPECT_EQ(parsed.recoveryReport.strictParseRuns, search.strictParseRuns);
  EXPECT_EQ(parsed.recoveryReport.recoveryAttemptRuns,
            search.recoveryAttemptRuns);
  EXPECT_EQ(parsed.recoveryReport.recoveryEdits,
            search.selectedAttempt.editCount);

  ASSERT_EQ(parsed.recoveryReport.lastRecoveryWindow.has_value(),
            !search.selectedWindows.empty());
  if (!search.selectedWindows.empty()) {
    const auto &lastWindow = search.selectedWindows.back();
    const auto &reportedWindow = *parsed.recoveryReport.lastRecoveryWindow;
    EXPECT_EQ(reportedWindow.beginOffset, lastWindow.beginOffset);
    EXPECT_EQ(reportedWindow.maxCursorOffset, lastWindow.maxCursorOffset);
    EXPECT_EQ(reportedWindow.backwardTokenCount, lastWindow.tokenCount);
    EXPECT_EQ(reportedWindow.forwardTokenCount, lastWindow.forwardTokenCount);
  }
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingPrefersFartherParseBeforeEditPosition) {
  detail::RecoveryAttempt farther;
  farther.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = false},
      .edits = {.editCost = 1,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 10},
      .progress = {.parsedLength = 24, .maxCursorOffset = 24},
  };

  detail::RecoveryAttempt nearer;
  nearer.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = false},
      .edits = {.editCost = 1,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 18},
      .progress = {.parsedLength = 20, .maxCursorOffset = 20},
  };

  EXPECT_TRUE(detail::is_better_recovery_attempt(farther, nearer));
  EXPECT_FALSE(detail::is_better_recovery_attempt(nearer, farther));
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingPrefersLaterEditWhenAttemptsOtherwiseTie) {
  detail::RecoveryAttempt earlier;
  earlier.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 1,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 8},
      .progress = {.parsedLength = 24, .maxCursorOffset = 24},
  };

  detail::RecoveryAttempt later;
  later.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 1,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 12},
      .progress = {.parsedLength = 24, .maxCursorOffset = 24},
  };

  EXPECT_TRUE(detail::is_better_recovery_attempt(later, earlier));
  EXPECT_FALSE(detail::is_better_recovery_attempt(earlier, later));
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingLetsNonCredibleFallbacksAdvanceByProgress) {
  detail::RecoveryAttempt earlierLocalFallback;
  earlierLocalFallback.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = false,
                    .credible = false,
                    .fullMatch = false},
      .edits = {.editCost = 1,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 18},
      .progress = {.parsedLength = 24, .maxCursorOffset = 24},
  };

  detail::RecoveryAttempt fartherFallback;
  fartherFallback.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = false,
                    .credible = false,
                    .fullMatch = false},
      .edits = {.editCost = 2,
                .editSpan = 4,
                .entryCount = 2,
                .firstEditOffset = 12},
      .progress = {.parsedLength = 31, .maxCursorOffset = 31},
  };

  EXPECT_TRUE(
      detail::is_better_recovery_attempt(fartherFallback, earlierLocalFallback));
  EXPECT_FALSE(
      detail::is_better_recovery_attempt(earlierLocalFallback, fartherFallback));
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingLetsLaterLocalRepairBeatSameCostSingleDelete) {
  detail::RecoveryAttempt wholeLineDelete;
  wholeLineDelete.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 8,
                .editSpan = 12,
                .entryCount = 1,
                .firstEditOffset = 40},
      .progress = {.parsedLength = 80, .maxCursorOffset = 80},
  };

  detail::RecoveryAttempt localRepair;
  localRepair.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 8,
                .editSpan = 12,
                .entryCount = 2,
                .firstEditOffset = 41},
      .progress = {.parsedLength = 80, .maxCursorOffset = 80},
  };

  EXPECT_TRUE(detail::is_better_recovery_attempt(localRepair, wholeLineDelete));
  EXPECT_FALSE(
      detail::is_better_recovery_attempt(wholeLineDelete, localRepair));
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingDoesNotPreferLaterEditWhenItCostsMoreAndSpansMore) {
  detail::RecoveryAttempt earlyLocalRepair;
  earlyLocalRepair.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 3,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 17},
      .progress = {.parsedLength = 31, .maxCursorOffset = 31},
  };

  detail::RecoveryAttempt laterRewrite;
  laterRewrite.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 9,
                .editSpan = 6,
                .entryCount = 3,
                .firstEditOffset = 18},
      .progress = {.parsedLength = 31, .maxCursorOffset = 31},
  };

  EXPECT_TRUE(
      detail::is_better_recovery_attempt(earlyLocalRepair, laterRewrite));
  EXPECT_FALSE(
      detail::is_better_recovery_attempt(laterRewrite, earlyLocalRepair));
}

TEST(RecoverySearchTest,
     GlobalRecoveryRankingPrefersFullerRecoveryBeforeCheaperPartialAttempt) {
  detail::RecoveryAttempt full;
  full.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = true},
      .edits = {.editCost = 12,
                .editSpan = 0,
                .entryCount = 2,
                .firstEditOffset = 8},
      .progress = {.parsedLength = 24, .maxCursorOffset = 24},
  };

  detail::RecoveryAttempt partial;
  partial.score = {
      .selection = {.entryRuleMatched = true,
                    .stable = true,
                    .credible = true,
                    .fullMatch = false},
      .edits = {.editCost = 1,
                .editSpan = 0,
                .entryCount = 1,
                .firstEditOffset = 12},
      .progress = {.parsedLength = 20, .maxCursorOffset = 20},
  };

  EXPECT_TRUE(detail::is_better_recovery_attempt(full, partial));
  EXPECT_FALSE(detail::is_better_recovery_attempt(partial, full));
}

TEST(RecoverySearchTest,
     EditableCandidateRankingUsesPostSkipProgressBeforeRawCursor) {
  detail::EditableRecoveryCandidate trailingTriviaHeavy{
      .matched = true,
      .cursorOffset = 18,
      .postSkipCursorOffset = 12,
      .editCost = 1,
      .editCount = 1,
      .firstEditOffset = 8,
  };
  detail::EditableRecoveryCandidate realProgress{
      .matched = true,
      .cursorOffset = 14,
      .postSkipCursorOffset = 14,
      .editCost = 1,
      .editCount = 1,
      .firstEditOffset = 8,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(realProgress),
      detail::editable_recovery_order_key(trailingTriviaHeavy),
      detail::RecoveryOrderProfile::Editable));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(trailingTriviaHeavy),
      detail::editable_recovery_order_key(realProgress),
      detail::RecoveryOrderProfile::Editable));
}

TEST(RecoverySearchTest,
     EditableCandidateContinuationUsesVisiblePostSkipAfterZeroWidthEdit) {
  const detail::EditableRecoveryCandidate candidate{
      .matched = true,
      .cursorOffset = 17,
      .postSkipCursorOffset = 18,
      .editCost = 1,
      .editCount = 1,
      .editSpan = 0,
      .firstEditOffset = 17,
  };

  EXPECT_TRUE(detail::continues_after_first_edit(candidate));
}

TEST(RecoverySearchTest, TerminalRecoveryOrderKeyProjectsSharedAxes) {
  const detail::TerminalRecoveryCandidate candidate{
      .kind = detail::TerminalRecoveryChoiceKind::DeleteScan,
      .anchorQuality = detail::TerminalAnchorQuality::DirectVisible,
      .cost = {.budgetCost = 4u,
               .primaryRankCost = 2u,
               .secondaryRankCost = 5u},
      .distance = 2u,
      .consumed = 7u,
      .substitutionCount = 0u,
      .operationCount = 1u,
  };

  const auto key = detail::terminal_recovery_order_key(candidate);
  EXPECT_TRUE(key.safety.matched);
  EXPECT_EQ(key.safety.strategyPriority, 2u);
  EXPECT_EQ(key.continuation.anchorQuality,
            detail::TerminalAnchorQuality::DirectVisible);
  EXPECT_EQ(key.continuation.consumedVisible, 7u);
  EXPECT_EQ(key.edits.primaryRankCost, 2u);
  EXPECT_EQ(key.edits.secondaryRankCost, 5u);
  EXPECT_EQ(key.edits.editCost, 4u);
  EXPECT_EQ(key.edits.distance, 2u);
}

TEST(RecoverySearchTest, EditableRecoveryOrderKeyProjectsSharedAxes) {
  const detail::EditableRecoveryCandidate candidate{
      .matched = true,
      .cursorOffset = 18,
      .postSkipCursorOffset = 18,
      .editCost = 3u,
      .editCount = 2u,
      .editSpan = 6u,
      .firstEditOffset = 9u,
  };

  const auto key = detail::editable_recovery_order_key(candidate);
  EXPECT_TRUE(key.safety.matched);
  EXPECT_TRUE(key.continuation.continuesAfterFirstEdit);
  EXPECT_EQ(key.continuation.postSkipCursorOffset, 18u);
  EXPECT_EQ(key.progress.cursorOffset, 18u);
  EXPECT_EQ(key.edits.editCost, 3u);
  EXPECT_EQ(key.edits.editCount, 2u);
  EXPECT_EQ(key.edits.editSpan, 6u);
  EXPECT_EQ(key.prefix.firstEditOffset, 9u);
}

TEST(RecoverySearchTest, TerminalOrderKeyComparatorMatchesCandidateComparator) {
  const detail::TerminalRecoveryCandidate lhs{
      .kind = detail::TerminalRecoveryChoiceKind::Replace,
      .anchorQuality = detail::TerminalAnchorQuality::DirectVisible,
      .cost = {.budgetCost = 2u,
               .primaryRankCost = 1u,
               .secondaryRankCost = 2u},
      .distance = 1u,
      .consumed = 5u,
      .substitutionCount = 1u,
      .operationCount = 1u,
  };
  const detail::TerminalRecoveryCandidate rhs{
      .kind = detail::TerminalRecoveryChoiceKind::DeleteScan,
      .anchorQuality = detail::TerminalAnchorQuality::AfterHiddenTrivia,
      .cost = {.budgetCost = 2u,
               .primaryRankCost = 1u,
               .secondaryRankCost = 2u},
      .distance = 1u,
      .consumed = 5u,
      .substitutionCount = 1u,
      .operationCount = 1u,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::terminal_recovery_order_key(lhs),
      detail::terminal_recovery_order_key(rhs),
      detail::RecoveryOrderProfile::Terminal));
}

TEST(RecoverySearchTest,
     ChoiceNormalizedComparatorMatchesCandidateComparatorWithoutOverride) {
  const detail::EditableRecoveryCandidate lhs{
      .matched = true,
      .cursorOffset = 18,
      .postSkipCursorOffset = 15,
      .editCost = 2u,
      .editCount = 1u,
      .editSpan = 1u,
      .firstEditOffset = 10u,
  };
  const detail::EditableRecoveryCandidate rhs{
      .matched = true,
      .cursorOffset = 19,
      .postSkipCursorOffset = 14,
      .editCost = 2u,
      .editCount = 1u,
      .editSpan = 2u,
      .firstEditOffset = 10u,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(lhs),
      detail::editable_recovery_order_key(rhs),
      detail::RecoveryOrderProfile::Choice));
}

TEST(RecoverySearchTest,
     ChoiceNormalizedComparatorMatchesCandidateComparatorForLaterCheaperContinuingEdit) {
  const detail::EditableRecoveryCandidate laterCheaperContinuingEdit{
      .matched = true,
      .cursorOffset = 30u,
      .postSkipCursorOffset = 30u,
      .editCost = 1u,
      .editCount = 1u,
      .editSpan = 1u,
      .firstEditOffset = 21u,
  };
  const detail::EditableRecoveryCandidate earlierCostlierEdit{
      .matched = true,
      .cursorOffset = 28u,
      .postSkipCursorOffset = 28u,
      .editCost = 2u,
      .editCount = 2u,
      .editSpan = 2u,
      .firstEditOffset = 18u,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(laterCheaperContinuingEdit),
      detail::editable_recovery_order_key(earlierCostlierEdit),
      detail::RecoveryOrderProfile::Choice));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(earlierCostlierEdit),
      detail::editable_recovery_order_key(laterCheaperContinuingEdit),
      detail::RecoveryOrderProfile::Choice));
  EXPECT_EQ(
      detail::is_better_normalized_recovery_order_key(
          detail::editable_recovery_order_key(laterCheaperContinuingEdit),
          detail::editable_recovery_order_key(earlierCostlierEdit),
          detail::RecoveryOrderProfile::Choice),
      detail::is_better_normalized_recovery_order_key(
          detail::editable_recovery_order_key(laterCheaperContinuingEdit),
          detail::editable_recovery_order_key(earlierCostlierEdit),
          detail::RecoveryOrderProfile::Choice));
}

TEST(RecoverySearchTest,
     ChoiceComparatorPrefersSmallerEditSpanAtSameBoundaryBeforeFartherOffset) {
  const detail::EditableRecoveryCandidate localInsert{
      .matched = true,
      .cursorOffset = 18u,
      .postSkipCursorOffset = 18u,
      .editCost = 1u,
      .editCount = 1u,
      .editSpan = 0u,
      .firstEditOffset = 18u,
  };
  const detail::EditableRecoveryCandidate deleteThenInsert{
      .matched = true,
      .cursorOffset = 24u,
      .postSkipCursorOffset = 24u,
      .editCost = 7u,
      .editCount = 2u,
      .editSpan = 6u,
      .firstEditOffset = 18u,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(localInsert),
      detail::editable_recovery_order_key(deleteThenInsert),
      detail::RecoveryOrderProfile::Choice));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(deleteThenInsert),
      detail::editable_recovery_order_key(localInsert),
      detail::RecoveryOrderProfile::Choice));
}

TEST(RecoverySearchTest,
     ChoiceComparatorDoesNotPreferLaterEditWhenItCostsMoreAndAddsMoreEdits) {
  const detail::EditableRecoveryCandidate earlyReplace{
      .matched = true,
      .cursorOffset = 26u,
      .postSkipCursorOffset = 26u,
      .editCost = 2u,
      .editCount = 1u,
      .editSpan = 2u,
      .firstEditOffset = 18u,
  };
  const detail::EditableRecoveryCandidate laterRewrite{
      .matched = true,
      .cursorOffset = 26u,
      .postSkipCursorOffset = 26u,
      .editCost = 9u,
      .editCount = 2u,
      .editSpan = 2u,
      .firstEditOffset = 21u,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(earlyReplace),
      detail::editable_recovery_order_key(laterRewrite),
      detail::RecoveryOrderProfile::Choice));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::editable_recovery_order_key(laterRewrite),
      detail::editable_recovery_order_key(earlyReplace),
      detail::RecoveryOrderProfile::Choice));
}

TEST(RecoverySearchTest,
     ChoiceComparatorBoundaryPreferenceStillPrefersCheaperSameStartEdit) {
  static const auto closeBrace = "}"_kw;
  const detail::EditableRecoveryCandidate continuingCurrent{
      .matched = true,
      .cursorOffset = 56u,
      .postSkipCursorOffset = 58u,
      .editCost = 96u,
      .editCount = 4u,
      .editSpan = 40u,
      .firstEditOffset = 16u,
  };
  const detail::EditableRecoveryCandidate preferredTailSkip{
      .matched = true,
      .cursorOffset = 59u,
      .postSkipCursorOffset = 59u,
      .editCost = 149u,
      .editCount = 5u,
      .editSpan = 42u,
      .firstEditOffset = 16u,
      .firstEditElement = std::addressof(closeBrace),
  };

  EXPECT_TRUE(
      detail::is_better_choice_recovery_candidate(
          continuingCurrent, preferredTailSkip,
          {.preferredBoundaryElement = std::addressof(closeBrace)}));
  EXPECT_FALSE(
      detail::is_better_choice_recovery_candidate(
          preferredTailSkip, continuingCurrent,
          {.preferredBoundaryElement = std::addressof(closeBrace)}));
}

TEST(RecoverySearchTest,
     ChoiceComparatorBoundaryPreferenceBreaksOnlySameStartTies) {
  static const auto closeBrace = "}"_kw;
  const detail::EditableRecoveryCandidate preferredBoundary{
      .matched = true,
      .cursorOffset = 32u,
      .postSkipCursorOffset = 32u,
      .editCost = 2u,
      .editCount = 1u,
      .editSpan = 0u,
      .firstEditOffset = 18u,
      .firstEditElement = std::addressof(closeBrace),
  };
  const detail::EditableRecoveryCandidate genericSameStart{
      .matched = true,
      .cursorOffset = 32u,
      .postSkipCursorOffset = 32u,
      .editCost = 2u,
      .editCount = 1u,
      .editSpan = 0u,
      .firstEditOffset = 18u,
      .firstEditElement = nullptr,
  };

  EXPECT_TRUE(
      detail::is_better_choice_recovery_candidate(
          preferredBoundary, genericSameStart,
          {.preferredBoundaryElement = std::addressof(closeBrace)}));
  EXPECT_FALSE(
      detail::is_better_choice_recovery_candidate(
          genericSameStart, preferredBoundary,
          {.preferredBoundaryElement = std::addressof(closeBrace)}));
}

TEST(RecoverySearchTest,
     ChoiceReplayComparatorPrefersLaterPrefixEditOverParseStartRewriteWhenCostDoesNotIncrease) {
  constexpr pegium::TextOffset parseStartOffset = 8u;
  const detail::EditableRecoveryCandidate parseStartRewrite{
      .matched = true,
      .cursorOffset = 20u,
      .postSkipCursorOffset = 20u,
      .editCost = 2u,
      .editCount = 2u,
      .firstEditOffset = 8u,
  };
  const detail::EditableRecoveryCandidate laterPrefixPreservingEdit{
      .matched = true,
      .cursorOffset = 20u,
      .postSkipCursorOffset = 20u,
      .editCost = 2u,
      .editCount = 1u,
      .firstEditOffset = 14u,
  };

  EXPECT_TRUE(detail::is_better_choice_recovery_candidate(
      laterPrefixPreservingEdit, parseStartRewrite,
      {.parseStartOffset = parseStartOffset}));
  EXPECT_FALSE(detail::is_better_choice_recovery_candidate(
      parseStartRewrite, laterPrefixPreservingEdit,
      {.parseStartOffset = parseStartOffset}));
}

TEST(RecoverySearchTest, ProgressOrderKeyComparatorMatchesCandidateComparator) {
  const detail::ProgressRecoveryCandidate lhs{
      .matched = true,
      .cursorOffset = 12u,
      .editCost = 1u,
  };
  const detail::ProgressRecoveryCandidate rhs{
      .matched = true,
      .cursorOffset = 11u,
      .editCost = 1u,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::progress_recovery_order_key(lhs),
      detail::progress_recovery_order_key(rhs),
      detail::RecoveryOrderProfile::Progress));
}

TEST(RecoverySearchTest, GlobalRecoveryOrderKeyProjectsSharedAxes) {
  const detail::RecoveryScore score{
      .selection =
          {
              .entryRuleMatched = true,
              .stable = true,
              .credible = true,
              .fullMatch = false,
          },
      .edits =
          {
              .editCost = 2u,
              .editSpan = 6u,
              .entryCount = 1u,
              .firstEditOffset = 12u,
          },
      .progress =
          {
              .parsedLength = 24u,
              .maxCursorOffset = 28u,
          },
  };

  const auto key = detail::recovery_attempt_order_key(score);
  EXPECT_TRUE(key.safety.matched);
  EXPECT_TRUE(key.prefix.entryRuleMatched);
  EXPECT_TRUE(key.prefix.stable);
  EXPECT_TRUE(key.prefix.credible);
  EXPECT_FALSE(key.prefix.fullMatch);
  EXPECT_EQ(key.prefix.firstEditOffset, 12u);
  EXPECT_TRUE(key.continuation.continuesAfterFirstEdit);
  EXPECT_EQ(key.edits.editCost, 2u);
  EXPECT_EQ(key.edits.editSpan, 6u);
  EXPECT_EQ(key.edits.entryCount, 1u);
  EXPECT_EQ(key.progress.parsedLength, 24u);
  EXPECT_EQ(key.progress.maxCursorOffset, 28u);
}

TEST(RecoverySearchTest, ScoreRecoveryAttemptTracksEditedSourceSpan) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 9u;
  attempt.editCount = 2u;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 9u,
                                .beginOffset = 9u,
                                .endOffset = 17u,
                                .element = nullptr,
                                .message = {}},
                               {.kind = ParseDiagnosticKind::Deleted,
                                .offset = 28u,
                                .beginOffset = 28u,
                                .endOffset = 31u,
                                .element = nullptr,
                                .message = {}}});

  detail::score_recovery_attempt(attempt);

  EXPECT_EQ(attempt.editTrace.firstEditOffset, 9u);
  EXPECT_EQ(attempt.editTrace.lastEditOffset, 31u);
  EXPECT_EQ(attempt.editTrace.editSpan, 22u);
  EXPECT_EQ(attempt.score.edits.firstEditOffset, 9u);
  EXPECT_EQ(attempt.score.edits.editSpan, 22u);
}

TEST(RecoverySearchTest, GlobalOrderKeyComparatorMatchesAttemptComparator) {
  detail::RecoveryAttempt lhs;
  lhs.score = {
      .selection =
          {
              .entryRuleMatched = true,
              .stable = false,
              .credible = false,
              .fullMatch = false,
          },
      .edits =
          {
              .editCost = 2u,
              .editSpan = 4u,
              .entryCount = 1u,
              .firstEditOffset = 10u,
          },
      .progress =
          {
              .parsedLength = 18u,
              .maxCursorOffset = 18u,
          },
  };
  detail::RecoveryAttempt rhs;
  rhs.score = lhs.score;
  rhs.score.edits.editCost = 3u;

  EXPECT_EQ(detail::is_better_recovery_attempt(lhs, rhs),
            detail::is_better_normalized_recovery_order_key(
                detail::recovery_attempt_order_key(lhs.score),
                detail::recovery_attempt_order_key(rhs.score),
                detail::RecoveryOrderProfile::Attempt));
}

TEST(RecoverySearchTest,
     StructuralComparatorPrefersContinuingEditOverWeakEarlierInsert) {
  const detail::StructuralProgressRecoveryCandidate continuingDelete{
      .matched = true,
      .cursorOffset = 22u,
      .editCost = 4u,
      .strategyPriority = 2u,
      .hadEdits = true,
      .continuesAfterFirstEdit = true,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Deleted,
      .firstEditOffset = 16u,
      .firstEditElement = nullptr,
  };
  const detail::StructuralProgressRecoveryCandidate weakInsert{
      .matched = true,
      .cursorOffset = 16u,
      .editCost = 1u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = false,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 16u,
      .firstEditElement = nullptr,
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::structural_progress_recovery_order_key(continuingDelete),
      detail::structural_progress_recovery_order_key(weakInsert),
      detail::RecoveryOrderProfile::StructuralProgress));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::structural_progress_recovery_order_key(weakInsert),
      detail::structural_progress_recovery_order_key(continuingDelete),
      detail::RecoveryOrderProfile::StructuralProgress));
}

TEST(RecoverySearchTest,
     StructuralComparatorKeepsWeakBoundaryLiteralInsertAvailable) {
  static const auto semicolon = ";"_kw;
  const detail::StructuralProgressRecoveryCandidate continuingDelete{
      .matched = true,
      .cursorOffset = 17u,
      .editCost = 4u,
      .strategyPriority = 2u,
      .hadEdits = true,
      .continuesAfterFirstEdit = true,
      .rewritesParseStartBoundary = true,
      .firstEditKind = ParseDiagnosticKind::Deleted,
      .firstEditOffset = 9u,
      .firstEditElement = nullptr,
  };
  const detail::StructuralProgressRecoveryCandidate weakLiteralInsert{
      .matched = true,
      .cursorOffset = 9u,
      .editCost = 1u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = false,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 9u,
      .firstEditElement = std::addressof(semicolon),
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::structural_progress_recovery_order_key(weakLiteralInsert),
      detail::structural_progress_recovery_order_key(continuingDelete),
      detail::RecoveryOrderProfile::StructuralProgress));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::structural_progress_recovery_order_key(continuingDelete),
      detail::structural_progress_recovery_order_key(weakLiteralInsert),
      detail::RecoveryOrderProfile::StructuralProgress));
}

TEST(RecoverySearchTest,
     StructuralComparatorPrefersContinuingEditOverWeakBoundaryLiteralInsert) {
  static const auto comma = ","_kw;
  static const auto closeParen = ")"_kw;
  const detail::StructuralProgressRecoveryCandidate continuingCommaInsert{
      .matched = true,
      .cursorOffset = 282u,
      .editCost = 2u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = true,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 277u,
      .firstEditElement = std::addressof(comma),
  };
  const detail::StructuralProgressRecoveryCandidate weakBoundaryClose{
      .matched = true,
      .cursorOffset = 276u,
      .editCost = 2u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = false,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 276u,
      .firstEditElement = std::addressof(closeParen),
  };

  EXPECT_TRUE(detail::is_better_normalized_recovery_order_key(
      detail::structural_progress_recovery_order_key(continuingCommaInsert),
      detail::structural_progress_recovery_order_key(weakBoundaryClose),
      detail::RecoveryOrderProfile::StructuralProgress));
  EXPECT_FALSE(detail::is_better_normalized_recovery_order_key(
      detail::structural_progress_recovery_order_key(weakBoundaryClose),
      detail::structural_progress_recovery_order_key(continuingCommaInsert),
      detail::RecoveryOrderProfile::StructuralProgress));
}

TEST(RecoverySearchTest,
     SameStartStructuralBoundaryInsertInvariantBeatsSameStartWeakCompetingEdit) {
  static const auto closeParen = ")"_kw;
  const detail::StructuralProgressRecoveryCandidate boundaryInsert{
      .matched = true,
      .cursorOffset = 276u,
      .editCost = 2u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = false,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 276u,
      .firstEditElement = std::addressof(closeParen),
  };
  const detail::StructuralProgressRecoveryCandidate competingWeakEdit{
      .matched = true,
      .cursorOffset = 276u,
      .editCost = 2u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = true,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 276u,
      .firstEditElement = nullptr,
  };

  EXPECT_TRUE(detail::is_better_structural_progress_recovery_candidate(
      boundaryInsert, competingWeakEdit, {.parseStartOffset = 276u}));
  EXPECT_FALSE(detail::is_better_structural_progress_recovery_candidate(
      competingWeakEdit, boundaryInsert, {.parseStartOffset = 276u}));
}

TEST(RecoverySearchTest,
     StructuralOrderKeyComparatorMatchesCandidateComparator) {
  const detail::StructuralProgressRecoveryCandidate lhs{
      .matched = true,
      .cursorOffset = 22u,
      .editCost = 4u,
      .strategyPriority = 2u,
      .hadEdits = true,
      .continuesAfterFirstEdit = true,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Deleted,
      .firstEditOffset = 16u,
      .firstEditElement = nullptr,
  };
  const detail::StructuralProgressRecoveryCandidate rhs{
      .matched = true,
      .cursorOffset = 16u,
      .editCost = 1u,
      .strategyPriority = 1u,
      .hadEdits = true,
      .continuesAfterFirstEdit = false,
      .rewritesParseStartBoundary = false,
      .firstEditKind = ParseDiagnosticKind::Inserted,
      .firstEditOffset = 16u,
      .firstEditElement = nullptr,
  };

  EXPECT_EQ(
      detail::is_better_structural_progress_recovery_candidate(lhs, rhs),
      detail::is_better_normalized_recovery_order_key(
          detail::structural_progress_recovery_order_key(lhs),
          detail::structural_progress_recovery_order_key(rhs),
          detail::RecoveryOrderProfile::StructuralProgress));
}

TEST(RecoverySearchTest, WordLiteralSingleSubstitutionCanRecoverGenerically) {
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>("service"_kw)};

  const auto result = pegium::test::Parse(entry, "servixe");

  EXPECT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
}

TEST(RecoverySearchTest,
     LongWordLiteralSingleSubstitutionCanRecoverGenerically) {
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>("catalogue"_kw)};

  const auto result = pegium::test::Parse(entry, "catalogoe");
  EXPECT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
}

TEST(RecoverySearchTest,
     FirstSelectableReplacementAtFileStartIsNotRejectedAsBoundaryRewrite) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", "module"_kw + assign<&RecoverySearchNode::name>(id)};

  const auto text = pegium::text::TextSnapshot::copy("modle basicMath");
  const auto run =
      detail::run_recovery_search(entry, skipper, ParseOptions{}, text);
  const auto json = detail::recovery_search_run_to_json(run);

  EXPECT_TRUE(run.selectedAttempt.entryRuleMatched) << json;
  EXPECT_TRUE(run.selectedAttempt.fullMatch) << json;
  EXPECT_EQ(run.selectedAttempt.status, detail::RecoveryAttemptStatus::Stable)
      << json;
  ASSERT_EQ(run.selectedAttempt.recoveryEdits.size(), 1u) << json;
  EXPECT_EQ(run.selectedAttempt.recoveryEdits.front().kind,
            ParseDiagnosticKind::Replaced)
      << json;
  EXPECT_EQ(run.selectedAttempt.recoveryEdits.front().offset, 0u) << json;
}

TEST(RecoverySearchTest,
     ParserUsesRecoveredButNotCredibleFallbackWhenNoSelectableAttemptExists) {
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
  const auto strictResult = run_strict_parse(entry, skipper, text);
  const auto failureAnalysis =
      inspect_failure(entry, skipper, text, strictResult.summary);
  const auto editFloorOffset =
      strictResult.summary.parsedLength != 0 ||
              !failureAnalysis.snapshot.hasFailureToken
          ? strictResult.summary.parsedLength
          : failureAnalysis.snapshot
                .failureLeafHistory[failureAnalysis.snapshot.failureTokenIndex]
                .beginOffset;
  const auto window = detail::compute_recovery_window(
      failureAnalysis.snapshot, options.recoveryWindowTokenCount,
      options.recoveryWindowTokenCount, editFloorOffset);
  const auto spec = build_attempt_spec({}, window);
  bool foundSelectableAttempt = false;
  auto attempt =
      detail::run_recovery_attempt(entry, skipper, options, text, spec);
  detail::classify_recovery_attempt(attempt);
  foundSelectableAttempt |= detail::is_selectable_recovery_attempt(attempt);

  EXPECT_TRUE(result.fullMatch);
  EXPECT_FALSE(foundSelectableAttempt);
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<RecoveryTripleNode>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->first, "one");
  EXPECT_EQ(typed->second, "two");
  EXPECT_EQ(typed->third, "three");
  ASSERT_EQ(result.parseDiagnostics.size(), 2u);
  EXPECT_EQ(result.parseDiagnostics[0].kind, ParseDiagnosticKind::Inserted);
  EXPECT_EQ(result.parseDiagnostics[1].kind, ParseDiagnosticKind::Inserted);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_TRUE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 2u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 2u);
}

TEST(RecoverySearchTest,
     RecoveredButNotCredibleAttemptWithinBudgetIsNotSelectable) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 12;
  attempt.completedRecoveryWindows = 1;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 12,
                                .beginOffset = 12,
                                .endOffset = 12,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     NarrowLocalGapInsertionWithoutStablePrefixCanBypassBudgetDowngrade) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.reachedRecoveryTarget = true;
  attempt.parsedLength = 28;
  attempt.lastVisibleCursorOffset = 28;
  attempt.maxCursorOffset = 31;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 12,
                                .beginOffset = 12,
                                .endOffset = 12,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status, detail::RecoveryAttemptStatus::Credible);
}

TEST(RecoverySearchTest,
     FullMatchLocalGapInsertionWithoutStablePrefixDoesNotBypassBudgetDowngrade) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = true;
  attempt.reachedRecoveryTarget = true;
  attempt.parsedLength = 28;
  attempt.lastVisibleCursorOffset = 28;
  attempt.maxCursorOffset = 31;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 12,
                                .beginOffset = 12,
                                .endOffset = 12,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     StableLocalGapInsertionWithoutStablePrefixDoesNotBypassBudgetDowngrade) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.stableAfterRecovery = true;
  attempt.reachedRecoveryTarget = true;
  attempt.parsedLength = 28;
  attempt.lastVisibleCursorOffset = 28;
  attempt.maxCursorOffset = 31;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 12,
                                .beginOffset = 12,
                                .endOffset = 12,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(detail::is_selectable_recovery_attempt(attempt));
}

TEST(RecoverySearchTest,
     MultiInsertWithoutStablePrefixDoesNotUseLocalGapBudgetExemption) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.reachedRecoveryTarget = true;
  attempt.parsedLength = 28;
  attempt.lastVisibleCursorOffset = 28;
  attempt.maxCursorOffset = 31;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 12,
                                .beginOffset = 12,
                                .endOffset = 12,
                                .element = nullptr,
                                .message = {}},
                               {.kind = ParseDiagnosticKind::Inserted,
                                .offset = 18,
                                .beginOffset = 18,
                                .endOffset = 18,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
}

TEST(RecoverySearchTest,
     DeleteOnlyFullMatchDeletingInputPrefixIsSelectableWithoutFallback) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = true;
  attempt.parsedLength = 53;
  attempt.lastVisibleCursorOffset = 53;
  attempt.maxCursorOffset = 53;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  attempt.replayWindows = {{.beginOffset = 0,
                            .editFloorOffset = 0,
                            .maxCursorOffset = 0,
                            .tokenCount = 1,
                            .forwardTokenCount = 1,
                            .visibleLeafBeginIndex = 0,
                            .stablePrefixOffset = 0,
                            .hasStablePrefix = false}};
  attempt.cst = make_attempt_cst("req login", {{24u, 27u}, {28u, 33u}});
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 0,
                                .beginOffset = 0,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status, detail::RecoveryAttemptStatus::Stable);
  EXPECT_TRUE(detail::is_selectable_recovery_attempt(attempt));
  EXPECT_FALSE(
      detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest,
     TrimmedTailFullMatchUsesFallbackSelectionInsteadOfSelectableRecovery) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = true;
  attempt.parsedLength = 20;
  attempt.lastVisibleCursorOffset = 20;
  attempt.maxCursorOffset = 20;
  attempt.trimmedVisibleTailToEof = true;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 4;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 20,
                                .beginOffset = 20,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status, detail::RecoveryAttemptStatus::Stable);
  EXPECT_FALSE(detail::is_selectable_recovery_attempt(attempt));
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest,
     DeleteOnlyPrefixProgressWithoutFullMatchCannotUseFallbackSelection) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = false;
  attempt.parsedLength = 41;
  attempt.lastVisibleCursorOffset = 41;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 48;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 0,
                                .beginOffset = 0,
                                .endOffset = 12,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(
      detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest,
     DeleteOnlyLocalGapProgressWithoutStablePrefixCanUseFallbackSelection) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = false;
  attempt.parsedLength = 17;
  attempt.lastVisibleCursorOffset = 17;
  attempt.maxCursorOffset = 34;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 1;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 8,
                                .beginOffset = 8,
                                .endOffset = 9,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest,
     InsertedPrefixProgressWithoutTailParseCanUseFallbackSelection) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.hasStablePrefix = true;
  attempt.stablePrefixOffset = 2;
  attempt.parsedLength = 21;
  attempt.lastVisibleCursorOffset = 21;
  attempt.maxCursorOffset = 29;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 2;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Inserted,
                                .offset = 3,
                                .beginOffset = 3,
                                .endOffset = 3,
                                .element = nullptr,
                                .message = {}},
                               {.kind = ParseDiagnosticKind::Inserted,
                                .offset = 21,
                                .beginOffset = 21,
                                .endOffset = 21,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest,
     ExtendedFallbackPreservingEarlierEditsCanUseFallbackSelection) {
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.hasStablePrefix = true;
  selectedAttempt.stablePrefixOffset = 10;
  selectedAttempt.parsedLength = 27;
  selectedAttempt.lastVisibleCursorOffset = 27;
  selectedAttempt.maxCursorOffset = 34;
  selectedAttempt.configuredMaxEditCost = 64;
  selectedAttempt.editCost = 1;
  set_recovery_edits(selectedAttempt, {{.kind = ParseDiagnosticKind::Inserted,
                                        .offset = 13,
                                        .beginOffset = 13,
                                        .endOffset = 13,
                                        .element = nullptr,
                                        .message = {}}});
  detail::classify_recovery_attempt(selectedAttempt);
  ASSERT_EQ(selectedAttempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);

  detail::RecoveryAttempt candidate;
  candidate.entryRuleMatched = true;
  candidate.hasStablePrefix = true;
  candidate.stablePrefixOffset = 10;
  candidate.parsedLength = 36;
  candidate.lastVisibleCursorOffset = 27;
  candidate.maxCursorOffset = 42;
  candidate.configuredMaxEditCost = 64;
  candidate.editCost = 4;
  set_recovery_edits(candidate, {{.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 13,
                                  .beginOffset = 13,
                                  .endOffset = 13,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 32,
                                  .beginOffset = 32,
                                  .endOffset = 32,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 34,
                                  .beginOffset = 34,
                                  .endOffset = 34,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 36,
                                  .beginOffset = 36,
                                  .endOffset = 36,
                                  .element = nullptr,
                                  .message = {}}});
  detail::classify_recovery_attempt(candidate);

  EXPECT_EQ(candidate.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(candidate, {}));
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(
      candidate, selectedAttempt));
}

TEST(RecoverySearchTest,
     StableReplayContractFallbackExtensionMustPreserveEarlierEdits) {
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.hasStablePrefix = true;
  selectedAttempt.stablePrefixOffset = 10;
  selectedAttempt.parsedLength = 27;
  selectedAttempt.lastVisibleCursorOffset = 27;
  selectedAttempt.maxCursorOffset = 34;
  selectedAttempt.stableAfterRecovery = true;
  selectedAttempt.configuredMaxEditCost = 64;
  selectedAttempt.editCost = 1;
  set_recovery_edits(selectedAttempt, {{.kind = ParseDiagnosticKind::Inserted,
                                        .offset = 13,
                                        .beginOffset = 13,
                                        .endOffset = 13,
                                        .element = nullptr,
                                        .message = {}}});
  detail::classify_recovery_attempt(selectedAttempt);
  ASSERT_EQ(selectedAttempt.status, detail::RecoveryAttemptStatus::Stable);

  detail::RecoveryAttempt candidate;
  candidate.entryRuleMatched = true;
  candidate.hasStablePrefix = true;
  candidate.stablePrefixOffset = 10;
  candidate.parsedLength = 36;
  candidate.lastVisibleCursorOffset = 27;
  candidate.maxCursorOffset = 42;
  candidate.configuredMaxEditCost = 64;
  candidate.editCost = 3;
  set_recovery_edits(candidate, {{.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 32,
                                  .beginOffset = 32,
                                  .endOffset = 32,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 34,
                                  .beginOffset = 34,
                                  .endOffset = 34,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 36,
                                  .beginOffset = 36,
                                  .endOffset = 36,
                                  .element = nullptr,
                                  .message = {}}});
  detail::classify_recovery_attempt(candidate);

  EXPECT_EQ(candidate.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(detail::satisfies_non_credible_fallback_contract(
      candidate, selectedAttempt));
}

TEST(RecoverySearchTest,
     StableReplayContractCanCarryLaterFallbackExtensionWhenEditsArePreserved) {
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.hasStablePrefix = true;
  selectedAttempt.stablePrefixOffset = 10;
  selectedAttempt.parsedLength = 27;
  selectedAttempt.lastVisibleCursorOffset = 27;
  selectedAttempt.maxCursorOffset = 34;
  selectedAttempt.stableAfterRecovery = true;
  selectedAttempt.configuredMaxEditCost = 64;
  selectedAttempt.editCost = 1;
  set_recovery_edits(selectedAttempt, {{.kind = ParseDiagnosticKind::Inserted,
                                        .offset = 13,
                                        .beginOffset = 13,
                                        .endOffset = 13,
                                        .element = nullptr,
                                        .message = {}}});
  detail::classify_recovery_attempt(selectedAttempt);
  ASSERT_EQ(selectedAttempt.status, detail::RecoveryAttemptStatus::Stable);

  detail::RecoveryAttempt candidate;
  candidate.entryRuleMatched = true;
  candidate.hasStablePrefix = true;
  candidate.stablePrefixOffset = 10;
  candidate.parsedLength = 36;
  candidate.lastVisibleCursorOffset = 27;
  candidate.maxCursorOffset = 42;
  candidate.configuredMaxEditCost = 64;
  candidate.editCost = 4;
  set_recovery_edits(candidate, {{.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 13,
                                  .beginOffset = 13,
                                  .endOffset = 13,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 32,
                                  .beginOffset = 32,
                                  .endOffset = 32,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 34,
                                  .beginOffset = 34,
                                  .endOffset = 34,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 36,
                                  .beginOffset = 36,
                                  .endOffset = 36,
                                  .element = nullptr,
                                  .message = {}}});
  detail::classify_recovery_attempt(candidate);

  EXPECT_EQ(candidate.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(
      candidate, selectedAttempt));
}

TEST(RecoverySearchTest,
     ExtendedFallbackCannotRewriteFirstCommittedSourceAtResumeFloor) {
  detail::RecoveryAttempt selectedAttempt;
  selectedAttempt.entryRuleMatched = true;
  selectedAttempt.hasStablePrefix = true;
  selectedAttempt.stablePrefixOffset = 10;
  selectedAttempt.parsedLength = 27;
  selectedAttempt.lastVisibleCursorOffset = 27;
  selectedAttempt.maxCursorOffset = 34;
  selectedAttempt.configuredMaxEditCost = 64;
  selectedAttempt.editCost = 1;
  set_recovery_edits(selectedAttempt, {{.kind = ParseDiagnosticKind::Inserted,
                                        .offset = 13,
                                        .beginOffset = 13,
                                        .endOffset = 13,
                                        .element = nullptr,
                                        .message = {}}});
  detail::classify_recovery_attempt(selectedAttempt);
  ASSERT_EQ(selectedAttempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);

  detail::RecoveryAttempt candidate;
  candidate.entryRuleMatched = true;
  candidate.hasStablePrefix = true;
  candidate.stablePrefixOffset = 10;
  candidate.parsedLength = 36;
  candidate.lastVisibleCursorOffset = 27;
  candidate.maxCursorOffset = 42;
  candidate.configuredMaxEditCost = 64;
  candidate.editCost = 4;
  set_recovery_edits(candidate, {{.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 13,
                                  .beginOffset = 13,
                                  .endOffset = 13,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 27,
                                  .beginOffset = 27,
                                  .endOffset = 27,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Deleted,
                                  .offset = 27,
                                  .beginOffset = 27,
                                  .endOffset = 28,
                                  .element = nullptr,
                                  .message = {}},
                                 {.kind = ParseDiagnosticKind::Inserted,
                                  .offset = 32,
                                  .beginOffset = 32,
                                  .endOffset = 32,
                                  .element = nullptr,
                                  .message = {}}});
  detail::classify_recovery_attempt(candidate);

  EXPECT_EQ(candidate.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_TRUE(detail::satisfies_non_credible_fallback_contract(candidate, {}));
  EXPECT_FALSE(detail::satisfies_non_credible_fallback_contract(
      candidate, selectedAttempt));
}

TEST(RecoverySearchTest,
     NonPrefixDeleteOnlyFullMatchCannotUseFallbackSelection) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = true;
  attempt.parsedLength = 53;
  attempt.lastVisibleCursorOffset = 53;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 12,
                                .beginOffset = 12,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(
      detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest,
     DeleteOnlyFullMatchWithoutVisibleContinuationCannotUseFallbackSelection) {
  detail::RecoveryAttempt attempt;
  attempt.entryRuleMatched = true;
  attempt.fullMatch = true;
  attempt.parsedLength = 24;
  attempt.lastVisibleCursorOffset = 0;
  attempt.configuredMaxEditCost = 64;
  attempt.editCost = 96;
  set_recovery_edits(attempt, {{.kind = ParseDiagnosticKind::Deleted,
                                .offset = 0,
                                .beginOffset = 0,
                                .endOffset = 24,
                                .element = nullptr,
                                .message = {}}});

  detail::classify_recovery_attempt(attempt);

  EXPECT_EQ(attempt.status,
            detail::RecoveryAttemptStatus::RecoveredButNotCredible);
  EXPECT_FALSE(
      detail::satisfies_non_credible_fallback_contract(attempt, {}));
}

TEST(RecoverySearchTest, EditBudgetCanDisableRecoveryAttempts) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoverySearchNode> entry{
      "Entry", assign<&RecoverySearchNode::name>(id) + "{"_kw + "}"_kw};

  ParseOptions options;
  options.maxRecoveryEditsPerAttempt = 0;
  const auto result =
      pegium::test::Parse(entry, "hello{", NoOpSkipper(), options);

  pegium::test::ExpectCst(result,
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

  pegium::test::ExpectCst(result,
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
          "end": 4,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
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
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Inserted);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_TRUE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 1u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->beginOffset, 0u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->maxCursorOffset, 4u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->backwardTokenCount, 1u);
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->forwardTokenCount, 2u);
}

TEST(RecoverySearchTest,
     ParserCanRecoverSeparatedErrorsWithinSingleStablePrefixWindow) {
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

  pegium::test::ExpectCst(result,
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
          "end": 8,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
        },
        {
          "begin": 8,
          "end": 13,
          "grammarSource": "third=ID",
          "text": "three"
        },
        {
          "begin": 14,
          "end": 14,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
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
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 2u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->backwardTokenCount, 1u);
  EXPECT_GE(result.recoveryReport.lastRecoveryWindow->forwardTokenCount, 2u);
  EXPECT_GE(result.recoveryReport.lastRecoveryWindow->beginOffset, 3u);
  EXPECT_GE(result.recoveryReport.lastRecoveryWindow->maxCursorOffset,
            result.recoveryReport.lastRecoveryWindow->beginOffset);
}

TEST(RecoverySearchTest,
     GlobalAttemptBudgetStillCapsLocalRecoveryToOneAttemptRun) {
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
  options.maxRecoveryAttempts = 1;

  const auto result =
      pegium::test::Parse(entry, "one;two three four", skipper, options);

  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_TRUE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryAttemptRuns, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 2u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
  EXPECT_EQ(result.recoveryReport.lastRecoveryWindow->backwardTokenCount, 1u);
  EXPECT_GE(result.recoveryReport.lastRecoveryWindow->forwardTokenCount, 2u);
}

TEST(RecoverySearchTest,
     ParserCanRecoverSeparatedErrorsWithTriviaWithinSingleStablePrefixWindow) {
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
      pegium::test::Parse(entry, "one;\n\ntwo three\n\nfour", skipper, options);

  pegium::test::ExpectCst(result,
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
          "end": 10,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
        },
        {
          "begin": 10,
          "end": 15,
          "grammarSource": "third=ID",
          "text": "three"
        },
        {
          "begin": 17,
          "end": 17,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
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
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 2u);
}

TEST(RecoverySearchTest,
     LaterRecoveryWindowGetsItsOwnLocalEditBudgetDuringReplay) {
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
  options.maxRecoveryEditsPerAttempt = 1;

  const auto result =
      pegium::test::Parse(entry, "one;two three four", skipper, options);

  EXPECT_TRUE(result.fullMatch);
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

  pegium::test::ExpectCst(result,
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
          "end": 16,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
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

TEST(RecoverySearchTest,
     ParserSingleWindowRepairsNearestLocalErrorBeforeStoppingAtNextOne) {
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

  pegium::test::ExpectCst(result,
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
          "end": 8,
          "grammarSource": "Literal",
          "recovered": true,
          "text": ""
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
  ASSERT_EQ(result.parseDiagnostics.size(), 2u);
  EXPECT_EQ(result.parseDiagnostics.front().kind, ParseDiagnosticKind::Inserted);
  EXPECT_EQ(result.parseDiagnostics.back().kind, ParseDiagnosticKind::Incomplete);
  EXPECT_EQ(result.parseDiagnostics.back().offset, 19u);
  EXPECT_EQ(result.parseDiagnostics.back().beginOffset, 18u);
  EXPECT_EQ(result.parseDiagnostics.back().endOffset, 23u);
  EXPECT_EQ(result.parseDiagnostics.back().message, "Unexpected input.");
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_FALSE(result.recoveryReport.fullRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryCount, 1u);
  EXPECT_EQ(result.recoveryReport.recoveryEdits, 1u);
  ASSERT_TRUE(result.recoveryReport.lastRecoveryWindow.has_value());
}

} // namespace
