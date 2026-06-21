/// Probe monotonicity property: the recoverable probe is a superset
/// of the strict probe for any grammar element. Whenever
/// `attempt_fast_probe` accepts at the current cursor,
/// `probe_recoverable_at_entry` must also accept there.
///
/// This file exercises the implication on a small representative
/// corpus (keyword, long keyword, terminal-rule) against a battery
/// of inputs (matching, typo, unrelated, empty). The property
/// underwrites the recovery dispatch's right to consult the
/// recoverable probe whenever a strict probe would have accepted.

#include "RecoveryTestSupport.hpp"
#include <pegium/core/parser/ParseAttempt.hpp>

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

using namespace pegium::parser;

namespace {

struct Fixture {
  pegium::text::TextSnapshot snapshot;
  std::unique_ptr<pegium::RootCstNode> cst;
  pegium::parser::detail::FailureHistoryRecorder failureRecorder;
  pegium::CstBuilder builder;
  Skipper skipper;
  RecoveryContext ctx;

  explicit Fixture(std::string_view text)
      : snapshot(pegium::text::TextSnapshot::copy(text)),
        cst(std::make_unique<pegium::RootCstNode>(snapshot)),
        failureRecorder{snapshot.view().data()},
        builder{*cst},
        skipper{SkipperBuilder().build()},
        ctx{builder, skipper, failureRecorder} {}
};

template <typename Element>
void expect_strict_implies_recoverable(Element &&element,
                                       std::string_view input,
                                       std::string_view caseName) {
  Fixture fixture{input};
  const bool strict = attempt_fast_probe(fixture.ctx, element);
  // Re-run from a fresh fixture: the strict probe may have left
  // visible residue (rewind is not used here on purpose — we want
  // to compare two probes from the same fresh state).
  Fixture fixtureForRecoverable{input};
  const bool recoverable =
      probe_recoverable_at_entry(element, fixtureForRecoverable.ctx);

  SCOPED_TRACE(testing::Message() << "case=" << caseName << " input=\""
                                  << input << "\" strict=" << strict
                                  << " recoverable=" << recoverable);
  // The implication: strict ⇒ recoverable.
  EXPECT_TRUE(!strict || recoverable);
}

} // namespace

TEST(IterationFactGenerator,
     recoverable_supersets_strict_across_canonical_inputs) {
  // Table-driven fold of the per-element-shape monotonicity cases.
  // Each row pairs an element-builder (run fresh per probe, so the
  // builder is invoked twice inside the helper) with one input and a
  // human-readable case label. Every distinct (element, input) pair
  // from the former per-shape tests survives as one row.
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};

  struct Row {
    std::string label;
    std::string_view input;
    std::function<bool(RecoveryContext &)> strictProbe;
    std::function<bool(RecoveryContext &)> recoverableProbe;
  };

  const auto keywordRow = [](std::string label, std::string_view input,
                             auto element) {
    return Row{std::move(label), input,
               [element](RecoveryContext &ctx) {
                 return attempt_fast_probe(ctx, element);
               },
               [element](RecoveryContext &ctx) {
                 return probe_recoverable_at_entry(element, ctx);
               }};
  };
  const auto terminalRow = [&id](std::string label, std::string_view input) {
    return Row{std::move(label), input,
               [&id](RecoveryContext &ctx) {
                 return attempt_fast_probe(ctx, id);
               },
               [&id](RecoveryContext &ctx) {
                 return probe_recoverable_at_entry(id, ctx);
               }};
  };

  const std::array rows = {
      // keyword strictly at cursor / typo / unrelated / EOF
      keywordRow("keyword_hello world", "hello world", "hello"_kw),
      keywordRow("keyword_hellp world", "hellp world", "hello"_kw),
      keywordRow("keyword_xyz unrelated", "xyz unrelated", "hello"_kw),
      keywordRow("keyword_", "", "hello"_kw),
      // long keyword: match / typo / garbage
      keywordRow("long_keyword_service rest", "service rest", "service"_kw),
      keywordRow("long_keyword_servce rest", "servce rest", "service"_kw),
      keywordRow("long_keyword_garbage rest", "garbage rest", "service"_kw),
      // terminal rule: identifier match / digits
      terminalRow("terminal_alpha rest", "alpha rest"),
      terminalRow("terminal_12345 rest", "12345 rest"),
  };

  for (const auto &row : rows) {
    Fixture fixtureStrict{row.input};
    const bool strict = row.strictProbe(fixtureStrict.ctx);
    // Re-run from a fresh fixture: the strict probe may have left
    // visible residue (rewind is not used here on purpose — we want
    // to compare two probes from the same fresh state).
    Fixture fixtureRecoverable{row.input};
    const bool recoverable = row.recoverableProbe(fixtureRecoverable.ctx);

    SCOPED_TRACE(testing::Message()
                 << "case=" << row.label << " input=\"" << row.input
                 << "\" strict=" << strict << " recoverable=" << recoverable);
    // The implication: strict ⇒ recoverable.
    EXPECT_TRUE(!strict || recoverable);
  }
}

TEST(IterationFactGenerator,
     monotonicity_holds_across_a_battery_of_canonical_inputs) {
  // A short battery exercising the canonical shapes against varied
  // inputs. Each subcase must respect the implication; the test
  // fails if any subcase breaks it. This serves as a property test
  // over keyword + terminal-rule shapes; the dedicated tests above
  // pin specific cases for clearer diagnostics on regressions.
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};

  for (const auto &input : {"hello", "hellp", "world", "", "  ", "?", "alpha",
                            "12", "service rest"}) {
    expect_strict_implies_recoverable(
        "hello"_kw, input, std::string{"battery_keyword_"} + input);
    expect_strict_implies_recoverable(
        id, input, std::string{"battery_terminal_"} + input);
  }
}
