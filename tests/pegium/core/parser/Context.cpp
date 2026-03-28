#include <gtest/gtest.h>
#include <memory>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/parser/RecoveryEditSupport.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

using namespace pegium::parser;

namespace {

struct StringNode : pegium::AstNode {
  string value;
};

struct ContextDispatchNode : pegium::AstNode {};

struct ContextDispatchExpr final : pegium::grammar::AbstractElement {
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = true;

  bool *parseContextUsed = nullptr;
  bool *trackedParseContextUsed = nullptr;

  ContextDispatchExpr(bool &parseContextFlag,
                      bool &trackedParseContextFlag) noexcept
      : parseContextUsed(&parseContextFlag),
        trackedParseContextUsed(&trackedParseContextFlag) {}

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override { return nullable; }
  void print(std::ostream &os) const override { os << "<context-dispatch>"; }
  constexpr const char *terminal(const char *begin) const noexcept {
    return begin;
  }

private:
  friend struct pegium::parser::detail::ParseAccess;

  bool parse_impl(ParseContext &ctx) const {
    *parseContextUsed = true;
    ctx.leaf(ctx.cursor() + 1, this);
    return true;
  }

  bool parse_impl(TrackedParseContext &ctx) const {
    *trackedParseContextUsed = true;
    ctx.leaf(ctx.cursor() + 1, this);
    return true;
  }

  template <typename Context>
    requires(!std::same_as<std::remove_cvref_t<Context>, ParseContext> &&
             !std::same_as<std::remove_cvref_t<Context>, TrackedParseContext>)
  bool parse_impl(Context &) const {
    return true;
  }
};

bool containsHiddenNode(const pegium::CstNodeView &node) {
  if (node.isHidden()) {
    return true;
  }
  for (const auto &child : node) {
    if (containsHiddenNode(child)) {
      return true;
    }
  }
  return false;
}

bool containsHiddenNode(const pegium::RootCstNode &root) {
  for (const auto &child : root) {
    if (containsHiddenNode(child)) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(ContextTest, DefaultContextSkipsNothing) {
  auto skipper = skip();
  auto builderHarness = pegium::test::makeCstBuilderHarness("abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  auto result = skipper.skip(input.begin(), builder);
  EXPECT_EQ(result, input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, IgnoreSkipsWhitespaceBeforeParsing) {
  TerminalRule<> ws{"WS", some(s)};
  auto skipper = skip(ignored(ws));
  auto builderHarness = pegium::test::makeCstBuilderHarness("   abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  auto result = skipper.skip(input.begin(), builder);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 3);

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, IgnoreAndHideAreApplied) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comment{"COMMENT", "//"_kw <=> &(eol | eof)};
  DataTypeRule<std::string> rule{"Rule", "a"_kw + "b"_kw};
  ParserRule<StringNode> root{"Root", assign<&StringNode::value>(rule)};

  const auto result = pegium::test::parse_rule_result(
      root, "a// comment\n   b", skip(ignored(ws), hidden(comment)));

  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<StringNode>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, "ab");
  ASSERT_TRUE(result.cst);
  EXPECT_TRUE(containsHiddenNode(*result.cst));
}

TEST(ContextTest, SkipOwnsTemporaryRules) {
  auto skipper = skip(ignored(TerminalRule<>{"WS", some(s)}),
                      hidden(TerminalRule<>{"Comma", ","_kw}));

  auto builderHarness = pegium::test::makeCstBuilderHarness(" ,x");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = skipper.skip(input.begin(), builder);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 2);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_TRUE((*it).isHidden());
  EXPECT_EQ((*it).getText(), ",");
}

TEST(ContextTest, ContextCanBeConvertedToParseContextWithoutOwning) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comma{"Comma", ","_kw};
  auto skipperContext = SkipperContext{std::tie(comma), std::tie(ws)};
  Skipper skipper = skipperContext;

  auto builderHarness = pegium::test::makeCstBuilderHarness(" ,x");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = skipper.skip(input.begin(), builder);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 2);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_TRUE((*it).isHidden());
  EXPECT_EQ((*it).getText(), ",");
}

TEST(ContextTest, ParseContextMarkAndRewindRestoreCursorAndBuilderState) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("ab");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  const auto skipper = NoOpSkipper();
  const auto literal = "a"_kw;

  ParseContext ctx{builder, skipper};
  const auto checkpoint = ctx.mark();
  ctx.leaf(input.begin() + 1, std::addressof(literal));

  ASSERT_EQ(ctx.cursorOffset(), 1u);
  ASSERT_EQ(ctx.lastVisibleCursorOffset(), 1u);

  ctx.rewind(checkpoint);

  EXPECT_EQ(ctx.cursorOffset(), 0u);
  EXPECT_EQ(ctx.lastVisibleCursorOffset(), 0u);
  auto *root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest,
     TrackedParseContextRewindRestoresFailureHistoryRecorderState) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("ab");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  const auto skipper = NoOpSkipper();
  const auto literal = "a"_kw;

  detail::FailureHistoryRecorder recorder(input.begin());
  TrackedParseContext ctx{builder, skipper, recorder,
                          pegium::utils::default_cancel_token};

  ASSERT_TRUE(ctx.isFailureHistoryRecordingEnabled());

  const auto checkpoint = ctx.mark();
  ctx.leaf(input.begin() + 1, std::addressof(literal));
  ctx.rewind(checkpoint);
  ctx.leaf(input.begin() + 1, std::addressof(literal));

  const auto snapshot = recorder.snapshot(ctx.maxCursorOffset());
  ASSERT_EQ(snapshot.failureLeafHistory.size(), 1u);
  EXPECT_EQ(snapshot.failureLeafHistory.front().beginOffset, 0u);
  EXPECT_EQ(snapshot.failureLeafHistory.front().endOffset, 1u);
}

TEST(ContextTest,
     RecoveryWindowForwardBudgetExtendsFromLaterEditsInCurrentAttempt) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  const auto skipper = NoOpSkipper();
  const auto a = "a"_kw;
  const auto b = "b"_kw;
  const auto c = "c"_kw;

  detail::FailureHistoryRecorder recorder(input.begin());
  RecoveryContext ctx{builder, skipper, recorder,
                      pegium::utils::default_cancel_token};
  ctx.setEditWindows({RecoveryContext::EditWindow{
      .beginOffset = 0,
      .editFloorOffset = 0,
      .maxCursorOffset = 0,
      .forwardTokenCount = 2,
  }});

  ctx.skip();
  ASSERT_TRUE(ctx.isInRecoveryPhase());

  ASSERT_TRUE(ctx.insertSynthetic(std::addressof(a)));
  ctx.leaf(input.begin() + 1, std::addressof(a));
  EXPECT_TRUE(ctx.isInRecoveryPhase());

  ASSERT_TRUE(ctx.insertSynthetic(std::addressof(b)));
  const auto replayForwardCounts = ctx.replayForwardTokenCounts();
  ASSERT_EQ(replayForwardCounts.size(), 1u);
  EXPECT_EQ(replayForwardCounts.front(), 3u);

  ctx.leaf(input.begin() + 2, std::addressof(b));
  EXPECT_TRUE(ctx.isInRecoveryPhase());

  ctx.leaf(input.begin() + 3, std::addressof(c));
  EXPECT_FALSE(ctx.isInRecoveryPhase());
}

TEST(ContextTest, RecoveryDeleteCanBridgeAdjacentHiddenTriviaIntoSameEdit) {
  TerminalRule<> ws{"WS", some(s)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("x    y");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  detail::FailureHistoryRecorder recorder(input.begin());
  RecoveryContext ctx{builder, skipper, recorder,
                      pegium::utils::default_cancel_token};
  ctx.setEditWindows({RecoveryContext::EditWindow{
      .beginOffset = 0,
      .editFloorOffset = 0,
      .maxCursorOffset = static_cast<pegium::TextOffset>(input.size()),
      .forwardTokenCount = 1,
  }});

  ctx.skip();
  ctx.skipAfterDelete = false;
  ASSERT_TRUE(ctx.deleteOneCodepoint());
  ASSERT_TRUE(ctx.extendLastDeleteThroughHiddenTrivia());
  EXPECT_EQ(ctx.cursorOffset(), 5u);
  ASSERT_TRUE(ctx.deleteOneCodepoint());
  EXPECT_EQ(ctx.cursorOffset(), 6u);
  EXPECT_EQ(ctx.recoveryEditCount(), 1u);

  const auto diagnostics = detail::materialize_syntax_diagnostics(
      detail::normalize_syntax_script(ctx.snapshotRecoveryEdits()));
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().kind, ParseDiagnosticKind::Deleted);
  EXPECT_EQ(diagnostics.front().beginOffset, 0u);
  EXPECT_EQ(diagnostics.front().endOffset, 6u);
}

TEST(ContextTest,
     MaterializeRecoveryEditsMergesCompatibleInsertedDiagnosticsAtSameOffset) {
  const std::vector<RecoveryContext::RecoveryEdit> edits{
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = 5,
       .beginOffset = 5,
       .endOffset = 5,
       .element = nullptr,
       .message = "expected terminator"},
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = 5,
       .beginOffset = 5,
       .endOffset = 5,
       .element = nullptr,
       .message = "expected terminator"},
  };

  const auto diagnostics = detail::materialize_syntax_diagnostics(
      detail::normalize_syntax_script(edits));

  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().kind, ParseDiagnosticKind::Inserted);
  EXPECT_EQ(diagnostics.front().offset, 5u);
  EXPECT_EQ(diagnostics.front().message, "expected terminator");
}

TEST(ContextTest, RunStrictParseUsesPlainContextWithoutFailureRecorder) {
  bool parseContextUsed = false;
  bool trackedParseContextUsed = false;
  ParserRule<ContextDispatchNode> entry{
      "Entry", ContextDispatchExpr{parseContextUsed, trackedParseContextUsed}};

  const auto text = pegium::text::TextSnapshot::copy("a");

  const detail::StrictFailureEngine strictFailureEngine;
  const auto result =
      strictFailureEngine.runStrictParse(entry, NoOpSkipper(), text);

  EXPECT_TRUE(result.summary.entryRuleMatched);
  EXPECT_TRUE(parseContextUsed);
  EXPECT_FALSE(trackedParseContextUsed);
}

TEST(ContextTest,
     RunStrictParseUsesTrackedContextWhenFailureRecorderIsPresent) {
  bool parseContextUsed = false;
  bool trackedParseContextUsed = false;
  ParserRule<ContextDispatchNode> entry{
      "Entry", ContextDispatchExpr{parseContextUsed, trackedParseContextUsed}};

  const auto text = pegium::text::TextSnapshot::copy("a");
  detail::FailureHistoryRecorder recorder(text.view().data());

  const detail::StrictFailureEngine strictFailureEngine;
  const auto result = strictFailureEngine.runStrictParse(
      entry, NoOpSkipper(), text, pegium::utils::default_cancel_token,
      &recorder);

  EXPECT_TRUE(result.summary.entryRuleMatched);
  EXPECT_FALSE(parseContextUsed);
  EXPECT_TRUE(trackedParseContextUsed);
}

TEST(
    ContextTest,
    RecoveryContextRewindRestoresLocalReplayFrontierWithoutRestoringFurthestExploredCursor) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("ab");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  const auto skipper = NoOpSkipper();
  const auto literal = "a"_kw;
  detail::FailureHistoryRecorder recorder(input.begin());
  RecoveryContext ctx{builder, skipper, recorder};

  EXPECT_EQ(ctx.cursorOffset(), 0u);
  EXPECT_EQ(ctx.localReplayMaxOffset(), 0u);
  EXPECT_EQ(ctx.furthestExploredOffset(), 0u);

  const auto checkpoint = ctx.mark();
  ctx.leaf(input.begin() + 1, std::addressof(literal));
  EXPECT_EQ(ctx.cursorOffset(), 1u);
  EXPECT_EQ(ctx.localReplayMaxOffset(), 1u);
  EXPECT_EQ(ctx.furthestExploredOffset(), 1u);

  ctx.rewind(checkpoint);

  EXPECT_EQ(ctx.cursorOffset(), 0u);
  EXPECT_EQ(ctx.localReplayMaxOffset(), 0u);
  EXPECT_EQ(ctx.furthestExploredOffset(), 1u);
}

TEST(ContextTest,
     RecoveryHelpersProbeAndSideEffectFreeParseRestoreLocalAndGlobalFrontiers) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("()");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  const auto skipper = NoOpSkipper();
  detail::FailureHistoryRecorder recorder(input.begin());
  RecoveryContext ctx{builder, skipper, recorder};
  auto delimited = "("_kw + "name"_kw + ")"_kw;
  auto literal = "("_kw;

  EXPECT_EQ(ctx.cursorOffset(), 0u);
  EXPECT_EQ(ctx.localReplayMaxOffset(), 0u);
  EXPECT_EQ(ctx.furthestExploredOffset(), 0u);

  EXPECT_TRUE(probe_started_without_edits(ctx, delimited));
  EXPECT_EQ(ctx.cursorOffset(), 0u);
  EXPECT_EQ(ctx.localReplayMaxOffset(), 0u);
  EXPECT_EQ(ctx.furthestExploredOffset(), 0u);

  EXPECT_TRUE(detail::attempt_parse_without_side_effects(ctx, literal));
  EXPECT_EQ(ctx.cursorOffset(), 0u);
  EXPECT_EQ(ctx.localReplayMaxOffset(), 0u);
  EXPECT_EQ(ctx.furthestExploredOffset(), 0u);
}
