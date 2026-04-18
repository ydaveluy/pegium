#include <gtest/gtest.h>

#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>

using namespace pegium::parser;

namespace {

struct FastProbeOnlyExpression : pegium::grammar::AbstractElement {
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = false;

  mutable std::size_t strictParseCalls = 0;
  mutable std::size_t fastProbeCalls = 0;

  [[nodiscard]] constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }

  void print(std::ostream &os) const override {
    os << "FastProbeOnlyExpression";
  }

  bool parse_impl(ParseContext &) const {
    ++strictParseCalls;
    return false;
  }

  bool parse_impl(TrackedParseContext &) const {
    ++strictParseCalls;
    return false;
  }

  bool parse_impl(RecoveryContext &) const {
    ++strictParseCalls;
    return false;
  }

  bool parse_impl(ExpectContext &) const {
    ++strictParseCalls;
    return false;
  }

  bool fast_probe_impl(TrackedParseContext &) const {
    ++fastProbeCalls;
    return true;
  }

  bool fast_probe_impl(RecoveryContext &) const {
    ++fastProbeCalls;
    return true;
  }
};

struct StartedWithoutEditsExpression : pegium::grammar::AbstractElement {
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = false;

  mutable std::size_t strictParseCalls = 0;
  mutable std::size_t fastProbeCalls = 0;

  [[nodiscard]] constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }

  void print(std::ostream &os) const override {
    os << "StartedWithoutEditsExpression";
  }

  bool parse_impl(TrackedParseContext &ctx) const {
    ++strictParseCalls;
    if (ctx.cursor() < ctx.end) {
      ctx.leaf(ctx.cursor() + 1, this);
    }
    return false;
  }

  bool parse_impl(RecoveryContext &ctx) const {
    return parse_impl(static_cast<TrackedParseContext &>(ctx));
  }

  bool parse_impl(ParseContext &) const {
    ++strictParseCalls;
    return false;
  }

  bool parse_impl(ExpectContext &) const {
    ++strictParseCalls;
    return false;
  }

  bool fast_probe_impl(TrackedParseContext &) const {
    ++fastProbeCalls;
    return false;
  }

  bool fast_probe_impl(RecoveryContext &) const {
    ++fastProbeCalls;
    return false;
  }
};

struct EntryRecoverableExpression : pegium::grammar::AbstractElement {
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = false;

  mutable std::size_t probeCalls = 0;

  [[nodiscard]] constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }

  void print(std::ostream &os) const override {
    os << "EntryRecoverableExpression";
  }

  bool parse_impl(ParseContext &) const { return false; }
  bool parse_impl(TrackedParseContext &) const { return false; }
  bool parse_impl(RecoveryContext &) const { return false; }
  bool parse_impl(ExpectContext &) const { return false; }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const {
    ++probeCalls;
    return ctx.allowInsert;
  }
};

struct ActiveRecoverySensitiveExpression : pegium::grammar::AbstractElement {
  static constexpr bool nullable = false;
  static constexpr bool isFailureSafe = false;

  mutable std::size_t probeCalls = 0;

  [[nodiscard]] constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }

  void print(std::ostream &os) const override {
    os << "ActiveRecoverySensitiveExpression";
  }

  bool parse_impl(ParseContext &) const { return false; }
  bool parse_impl(TrackedParseContext &) const { return false; }
  bool parse_impl(RecoveryContext &) const { return false; }
  bool parse_impl(ExpectContext &) const { return false; }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const {
    ++probeCalls;
    return ctx.isActiveRecovery(this);
  }
};

} // namespace

TEST(ParseAttemptTest, ProbeStartedWithoutEditsUsesFastProbeBeforeStrictParse) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  auto skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();

  FastProbeOnlyExpression expression;

  EXPECT_TRUE(probe_started_without_edits(ctx, expression));
  EXPECT_EQ(expression.fastProbeCalls, 1u);
  EXPECT_EQ(expression.strictParseCalls, 0u);
}

TEST(ParseAttemptTest, FastProbeMemoizesWithinRecoveryContext) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  auto skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();
  const auto *const startCursor = ctx.cursor();

  FastProbeOnlyExpression expression;

  EXPECT_TRUE(attempt_fast_probe(ctx, expression));
  EXPECT_TRUE(attempt_fast_probe(ctx, expression));
  EXPECT_EQ(expression.fastProbeCalls, 1u);
  EXPECT_EQ(expression.strictParseCalls, 0u);
  EXPECT_EQ(ctx.cursor(), startCursor);
  EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
}

TEST(ParseAttemptTest, StartedWithoutEditsMemoizesWithinRecoveryContext) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  auto skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();
  const auto *const startCursor = ctx.cursor();

  StartedWithoutEditsExpression expression;

  EXPECT_TRUE(probe_started_without_edits(ctx, expression));
  EXPECT_TRUE(probe_started_without_edits(ctx, expression));
  EXPECT_EQ(expression.fastProbeCalls, 1u);
  EXPECT_EQ(expression.strictParseCalls, 1u);
  EXPECT_EQ(ctx.cursor(), startCursor);
  EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
}

TEST(ParseAttemptTest, EntryRecoverableMemoInvalidatesWhenPolicyChanges) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  auto skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.setEditWindows({RecoveryContext::EditWindow{
      .beginOffset = 0,
      .editFloorOffset = 0,
      .maxCursorOffset = 0,
      .forwardTokenCount = 1,
  }});
  ctx.skip();

  EntryRecoverableExpression expression;

  EXPECT_TRUE(probe_recoverable_at_entry(expression, ctx));
  EXPECT_TRUE(probe_recoverable_at_entry(expression, ctx));
  EXPECT_EQ(expression.probeCalls, 1u);

  auto noInsertGuard = ctx.withEditPermissions(false, ctx.allowDelete);
  (void)noInsertGuard;
  EXPECT_FALSE(probe_recoverable_at_entry(expression, ctx));
  EXPECT_EQ(expression.probeCalls, 2u);
}

TEST(ParseAttemptTest, ActiveRecoverySignatureTracksRecoveryStack) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  auto skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();

  FastProbeOnlyExpression outerExpression;
  StartedWithoutEditsExpression innerExpression;

  const auto baseSignature = ctx.activeRecoverySignature();
  {
    auto outerGuard = ctx.enterActiveRecovery(&outerExpression);
    (void)outerGuard;
    const auto outerSignature = ctx.activeRecoverySignature();
    EXPECT_NE(outerSignature, baseSignature);

    auto innerGuard = ctx.enterActiveRecovery(&innerExpression);
    (void)innerGuard;
    EXPECT_NE(ctx.activeRecoverySignature(), outerSignature);
  }

  EXPECT_EQ(ctx.activeRecoverySignature(), baseSignature);
}

TEST(ParseAttemptTest, EntryRecoverableMemoInvalidatesWhenActiveRecoveryChanges) {
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  auto skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, recorder};
  ctx.skip();

  ActiveRecoverySensitiveExpression expression;

  EXPECT_FALSE(probe_recoverable_at_entry(expression, ctx));
  EXPECT_FALSE(probe_recoverable_at_entry(expression, ctx));
  EXPECT_EQ(expression.probeCalls, 1u);

  auto activeGuard = ctx.enterActiveRecovery(&expression);
  (void)activeGuard;
  EXPECT_TRUE(probe_recoverable_at_entry(expression, ctx));
  EXPECT_EQ(expression.probeCalls, 2u);
}
