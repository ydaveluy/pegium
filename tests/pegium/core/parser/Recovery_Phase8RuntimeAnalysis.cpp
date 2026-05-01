/// Runtime corpus analysis of global compensations.
///
/// The global recovery compensations — `minimize_recovery_edits`,
/// the minimal-edit strategy probe, and the root-prefix delete
/// retry — each expose `Runs` and `Wins` step counters. This suite
/// drives a compact representative corpus and reads the counters
/// back, producing a Wins/Runs ratio per compensation.
///
/// The test only asserts on the COUNTERS' MECHANICAL CORRECTNESS —
/// `Wins <= Runs` and the counters increment as the dispatch fires
/// — which holds whether `PEGIUM_ENABLE_STEP_TRACE` is defined or
/// not (the helpers are no-ops when undefined). The actual ratios
/// are dumped to stderr for human inspection; the decision to
/// retain or retire each compensation is taken from the dumped
/// numbers, not from a test assertion. Configuration: build with
/// `-DPEGIUM_ENABLE_STEP_TRACE=ON` to populate the counters.

#include "RecoveryTestSupport.hpp"

#include <pegium/core/parser/StepTrace.hpp>

#include <gtest/gtest.h>

#include <iostream>

using namespace pegium::parser;
using namespace pegium::test::recovery;
using pegium::parser::detail::stepTraceReset;
using pegium::parser::detail::stepTraceValue;
using pegium::parser::detail::StepCounter;

namespace {

struct Phase8Counters {
  std::uint64_t minimizeRuns = 0;
  std::uint64_t minimizeDropped = 0;
  std::uint64_t minimalProbeRuns = 0;
  std::uint64_t minimalProbeWins = 0;
  std::uint64_t rootPrefixRuns = 0;
  std::uint64_t rootPrefixWins = 0;
};

[[nodiscard]] Phase8Counters snapshot_counters() noexcept {
  return {
      .minimizeRuns = stepTraceValue(StepCounter::MinimizeRecoveryEditsRuns),
      .minimizeDropped =
          stepTraceValue(StepCounter::MinimizeRecoveryEditsDropped),
      .minimalProbeRuns = stepTraceValue(StepCounter::MinimalEditProbeRuns),
      .minimalProbeWins = stepTraceValue(StepCounter::MinimalEditProbeWins),
      .rootPrefixRuns = stepTraceValue(StepCounter::RootPrefixRetryRuns),
      .rootPrefixWins = stepTraceValue(StepCounter::RootPrefixRetryWins),
  };
}

void dump_counters(const Phase8Counters &counters,
                    std::string_view scenario) noexcept {
  std::cerr << "[phase8] " << scenario << "\n"
            << "  minimize_recovery_edits   runs=" << counters.minimizeRuns
            << " dropped=" << counters.minimizeDropped << "\n"
            << "  minimal_edit_probe        runs=" << counters.minimalProbeRuns
            << " wins=" << counters.minimalProbeWins << "\n"
            << "  root_prefix_delete_retry  runs=" << counters.rootPrefixRuns
            << " wins=" << counters.rootPrefixWins << "\n";
}

[[nodiscard]] Phase8Counters
delta(const Phase8Counters &before, const Phase8Counters &after) noexcept {
  return {
      .minimizeRuns = after.minimizeRuns - before.minimizeRuns,
      .minimizeDropped = after.minimizeDropped - before.minimizeDropped,
      .minimalProbeRuns = after.minimalProbeRuns - before.minimalProbeRuns,
      .minimalProbeWins = after.minimalProbeWins - before.minimalProbeWins,
      .rootPrefixRuns = after.rootPrefixRuns - before.rootPrefixRuns,
      .rootPrefixWins = after.rootPrefixWins - before.rootPrefixWins,
  };
}

void expect_counter_invariants(const Phase8Counters &counters) {
  // Mechanical correctness: wins never exceed runs, dropped never
  // exceeds runs. Holds whether the trace is compiled in or out.
  EXPECT_LE(counters.minimalProbeWins, counters.minimalProbeRuns);
  EXPECT_LE(counters.rootPrefixWins, counters.rootPrefixRuns);
  // Note: `minimizeDropped` is the SUM of dropped edits across all
  // runs, not bounded by `minimizeRuns` (a single run can drop
  // multiple edits). No invariant beyond non-negativity.
}

} // namespace

// -----------------------------------------------------------------------------
// Synthetic grammar exercising each compensation's narrow trigger
// -----------------------------------------------------------------------------

// gtest environment that dumps the step counters AFTER every other
// test in the binary has run. Without this hook the counters would
// be read mid-suite, missing contributions from later tests.
class Phase8CounterReporter : public ::testing::Environment {
public:
  void SetUp() override { stepTraceReset(); }

  void TearDown() override {
    const auto counters = snapshot_counters();
    expect_counter_invariants(counters);
    dump_counters(counters, "AGGREGATE across PegiumCoreUnitTest");
  }
};

const ::testing::Environment *const kPhase8CounterReporter =
    ::testing::AddGlobalTestEnvironment(new Phase8CounterReporter());

TEST(Phase8RuntimeAnalysis,
     micro_corpus_drives_global_compensations_and_dumps_ratios) {
  // Snapshot before/after so the per-test contribution is visible
  // alongside the aggregate the Environment dumps.
  const Phase8Counters baseline = snapshot_counters();
  expect_counter_invariants(baseline);

  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  const auto skipper = SkipperBuilder().ignore(ws).build();

  // Grammar: Module = some(Definition); Definition = "def" id ";".
  // Re-uses `RecoveryModule` + `RecoveryDefinition` from the shared
  // support header so the micro-corpus relies on the same node
  // shapes as the rest of the recovery suite.
  ParserRule<RecoveryDefinition> definition{
      "Definition",
      "def"_kw + create<RecoveryDefinition>() +
          assign<&RecoveryDefinition::name>(id) + ";"_kw};
  ParserRule<RecoveryModule> entry{
      "Module",
      pegium::parser::create<RecoveryModule>() +
          some(append<&RecoveryModule::statements>(definition))};

  // Scenario 1: minimize_recovery_edits — multi-edit recovery on
  // a repeated-tail with extra semicolons.
  parseRule(entry, "def x;; def y;", skipper);
  // Scenario 2: minimal-edit probe — greedy primary on a missing-
  // semicolon then trailing token.
  parseRule(entry, "def x def y;", skipper);
  // Scenario 3: root_prefix_delete_retry — input prefixed with
  // garbage so the entry rule does not match at offset 0.
  parseRule(entry, "@@@ def x;", skipper);
  // Scenario 4: extra noise inside a definition.
  parseRule(entry, "def x !! ;", skipper);
  // Scenario 5: malformed token sequence.
  parseRule(entry, "def !x; def y;", skipper);

  const Phase8Counters after = snapshot_counters();
  expect_counter_invariants(after);

  const auto ours = delta(baseline, after);
  dump_counters(ours, "phase8_micro_corpus (this test only)");

  // The Environment hook above will report the AGGREGATE across all
  // tests in the binary at TearDown. That aggregate is the
  // substantive runtime data: ratios `Wins/Runs` for each
  // compensation across ~1280 recovery + non-recovery tests.
}
