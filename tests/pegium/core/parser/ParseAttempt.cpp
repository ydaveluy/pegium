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
