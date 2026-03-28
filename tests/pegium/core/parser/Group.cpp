#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
using namespace pegium::parser;

TEST(GroupTest, ParseTerminalConsumesElementsInSequence) {
  auto group = ":"_kw + ";"_kw;
  std::string input = ":;x";

  auto result = group.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - (input).c_str(), 2);
}

TEST(GroupTest, ParseRuleFailureDoesNotRollbackLocally) {
  auto group = ":"_kw + ";"_kw;
  auto context = SkipperBuilder().build();

  auto koBuilderHarness = pegium::test::makeCstBuilderHarness("::");
  auto &koBuilder = koBuilderHarness.builder;
  ParseContext koState{koBuilder, context};
  auto ko = parse(group, koState);
  EXPECT_FALSE(ko);
  auto koRoot = koBuilder.getRootCstNode();
  EXPECT_NE(koRoot->begin(), koRoot->end());

  auto okBuilderHarness = pegium::test::makeCstBuilderHarness(":;");
  auto &okBuilder = okBuilderHarness.builder;
  ParseContext okState{okBuilder, context};
  auto ok = parse(group, okState);
  EXPECT_TRUE(ok);
  auto okRoot = okBuilder.getRootCstNode();
  EXPECT_NE(okRoot->begin(), okRoot->end());
}

TEST(GroupTest, WithLocalSkipperCanMatchInternalSeparators) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto group = ("a"_kw + "b"_kw).skip(ignored(ws));

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b");
  auto &builder = builderHarness.builder;
  ParseContext state{builder, defaultSkipper};

  EXPECT_TRUE(parse(group, state));
  EXPECT_EQ(state.cursorOffset(), 5u);
}

TEST(GroupTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto group = ("a"_kw + "b"_kw).skip(ignored(ws));
  auto trailingLiteral = "c"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b   c");
  auto &builder = builderHarness.builder;
  ParseContext state{builder, defaultSkipper};

  ASSERT_TRUE(parse(group, state));
  EXPECT_EQ(state.cursorOffset(), 5u);

  const auto beforeSkip = state.cursorOffset();
  state.skip();
  EXPECT_EQ(state.cursorOffset(), beforeSkip);
  EXPECT_FALSE(parse(trailingLiteral, state));
}

TEST(GroupTest, ParseTerminalFailsWhenAnElementDoesNotMatch) {
  auto group = "ab"_kw + "cd"_kw;
  std::string input = "abX";

  auto result = group.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(GroupTest, FastProbeSkipsNullablePrefixBeforeMandatoryElement) {
  auto group = option("many"_kw) + "name"_kw + ":"_kw;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("name:");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    EXPECT_TRUE(attempt_fast_probe(ctx, group));
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("other");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    EXPECT_FALSE(attempt_fast_probe(ctx, group));
  }
}

TEST(GroupTest,
     ProbeRecoverableCanSeeImmediateSuffixAfterSingleMissingRequiredPrefix) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  auto group = ","_kw + id;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("name");
    auto &builder = builderHarness.builder;
    detail::FailureHistoryRecorder recorder(builder.input_begin());
    RecoveryContext ctx{builder, skipper, recorder};
    ctx.skip();
    EXPECT_TRUE(probe_locally_recoverable(group, ctx));
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(")");
    auto &builder = builderHarness.builder;
    detail::FailureHistoryRecorder recorder(builder.input_begin());
    RecoveryContext ctx{builder, skipper, recorder};
    ctx.skip();
    EXPECT_FALSE(probe_locally_recoverable(group, ctx));
  }
}

TEST(GroupTest, ScopedLeadingTerminalInsertDoesNotLeakOutsideGuard) {
  auto skipper = SkipperBuilder().build();
  auto group = "::"_kw + "name"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("name");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();

  const auto checkpoint = ctx.mark();
  {
    auto leadingInsertScope = ctx.withLeadingTerminalInsertScope();
    EXPECT_TRUE(parse(group, ctx));
  }

  ctx.rewind(checkpoint);
  EXPECT_FALSE(parse(group, ctx));
}

TEST(GroupTest,
     ScopedLeadingTerminalInsertCanContinueLocallyAfterEarlierEditInSameWindow) {
  auto skipper = SkipperBuilder().build();
  auto group = ","_kw + "name"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("name");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();
  ASSERT_TRUE(ctx.insertSyntheticGapAt(ctx.cursor()));

  const auto checkpoint = ctx.mark();
  {
    auto noDeleteGuard = ctx.withEditPermissions(true, false);
    (void)noDeleteGuard;
    auto leadingInsertScope = ctx.withLeadingTerminalInsertScope();
    (void)leadingInsertScope;
    EXPECT_TRUE(parse(group, ctx));
  }

  ctx.rewind(checkpoint);
  {
    auto leadingInsertScope = ctx.withLeadingTerminalInsertScope();
    (void)leadingInsertScope;
    EXPECT_FALSE(parse(group, ctx));
  }

  ctx.rewind(checkpoint);
  {
    auto noDeleteGuard = ctx.withEditPermissions(true, false);
    (void)noDeleteGuard;
    EXPECT_FALSE(parse(group, ctx));
  }
}

TEST(GroupTest,
     ScopedLeadingTerminalInsertCanContinueLocallyAfterHiddenTrivia) {
  auto skipper = SkipperBuilder().build();
  auto group = ","_kw + "name"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness(" name");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();
  ASSERT_TRUE(ctx.insertSyntheticGapAt(ctx.cursor()));

  const auto checkpoint = ctx.mark();
  {
    auto noDeleteGuard = ctx.withEditPermissions(true, false);
    (void)noDeleteGuard;
    auto leadingInsertScope = ctx.withLeadingTerminalInsertScope();
    (void)leadingInsertScope;
    EXPECT_TRUE(parse(group, ctx));
  }

  ctx.rewind(checkpoint);
  {
    auto leadingInsertScope = ctx.withLeadingTerminalInsertScope();
    (void)leadingInsertScope;
    EXPECT_FALSE(parse(group, ctx));
  }

  ctx.rewind(checkpoint);
  {
    auto noDeleteGuard = ctx.withEditPermissions(true, false);
    (void)noDeleteGuard;
    EXPECT_FALSE(parse(group, ctx));
  }
}
