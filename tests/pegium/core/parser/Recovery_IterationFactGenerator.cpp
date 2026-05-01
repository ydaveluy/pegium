/// Iteration boundary fact-generator (design assumption check).
///
/// The closed list of impossible `IterationBoundaryFacts`
/// combinations rests on the design assumption that the recoverable
/// probe is a superset of the strict probe for any grammar element:
/// whenever `attempt_fast_probe` accepts at the current cursor,
/// `probe_recoverable_at_entry` must also accept there. The two
/// impossibilities (`first_strict_implies_first_recoverable` and
/// `follow_strict_implies_follow_recoverable`) follow directly from
/// this monotonicity.
///
/// This file exercises the implication on a small representative
/// corpus (keyword, long keyword, terminal-rule) against a battery
/// of inputs (matching, typo, unrelated, empty). A regression here
/// means the impossibility list lost its foundation and must be
/// revisited.
///
/// Per-element first-set monotonicity transfers directly to the
/// follow-set fact: Group's follow probe is `attempt_fast_probe` /
/// `probe_recoverable_at_entry` on the next element, exactly the
/// pair tested below — so `followStrict ⇒ followRecoverable` is
/// implied.

#include "RecoveryTestSupport.hpp"
#include <pegium/core/parser/ParseAttempt.hpp>

#include <string>
#include <string_view>

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
     keyword_recoverable_supersets_strict_across_canonical_inputs) {
  static constexpr std::array kInputs = {
      std::string_view{"hello world"},  // keyword strictly at cursor
      std::string_view{"hellp world"},  // typo at cursor
      std::string_view{"xyz unrelated"}, // unrelated at cursor
      std::string_view{""},              // EOF
  };
  for (const auto input : kInputs) {
    expect_strict_implies_recoverable(
        "hello"_kw, input, std::string{"keyword_"} + std::string{input});
  }
}

TEST(IterationFactGenerator,
     long_keyword_recoverable_supersets_strict_across_canonical_inputs) {
  static constexpr std::array kInputs = {
      std::string_view{"service rest"},
      std::string_view{"servce rest"},
      std::string_view{"garbage rest"},
  };
  for (const auto input : kInputs) {
    expect_strict_implies_recoverable(
        "service"_kw, input, std::string{"long_keyword_"} + std::string{input});
  }
}

TEST(IterationFactGenerator,
     terminal_rule_recoverable_supersets_strict_across_canonical_inputs) {
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  static constexpr std::array kInputs = {
      std::string_view{"alpha rest"},
      std::string_view{"12345 rest"},
  };
  for (const auto input : kInputs) {
    expect_strict_implies_recoverable(
        id, input, std::string{"terminal_"} + std::string{input});
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
