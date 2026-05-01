#pragma once

/// `IterationBoundaryDecision`: the closed boundary decision a
/// `Repetition` makes once, before it enumerates any local recovery
/// candidate.
///
/// Every `Repetition` recovery attempt selects exactly one
/// `IterationBoundaryDecision` from the closed precedence table below
/// before producing any candidate. The decision determines which
/// candidate families the iteration is even allowed to enumerate:
///
///   - `StopCleanly`              no local edit candidate
///   - `ContinueStrict`           no-edit strict continuation only
///   - `ContinueRecoverable`      local repair without crossing the
///                                strict parent follow
///   - `RepairStartedIteration`   delete/insert retry on an iteration
///                                that already started
///   - `Resync`                   last-resort recovery
///   - `RejectToParentFollow`     no local candidate; defer to parent
///
/// The decision is computed from 6 boolean structural facts only —
/// budget concerns, candidate enumeration, and admission rules live
/// outside the precedence table. This isolation is what makes the
/// table closed and exhaustively testable.
///
/// Strict-path discipline: this header is recovery-side. It must
/// never be constructed on the strict-only nominal path.

#include <cstdint>
#include <type_traits>

namespace pegium::parser::detail {

/// Closed list of decisions for the `Repetition` boundary precedence
/// table. The values are exhaustive and frozen by the density ceiling
/// rule: a new value requires removing or merging an existing one.
enum class IterationBoundaryDecision : std::uint8_t {
  /// No local edit candidate is allowed. The iteration stops without
  /// recovery; control returns to the parent.
  StopCleanly,

  /// The iteration continues strictly: the element accepts at the
  /// current cursor without any edit. No edit candidate is produced.
  ContinueStrict,

  /// The iteration continues with a recoverable local repair: the
  /// element accepts in recoverable mode, and the strict parent
  /// follow does not block.
  ContinueRecoverable,

  /// The iteration was already started strictly and a local repair
  /// can be observed without crossing the strict parent follow.
  /// Delete/insert retry candidates on the started iteration are
  /// admissible.
  RepairStartedIteration,

  /// Last-resort recovery: no other rule applies, no strict boundary
  /// is in scope, the iteration falls back to a budgeted resync scan.
  Resync,

  /// The only producible local candidate would cross the strict
  /// parent follow. The iteration produces nothing locally and
  /// defers to the parent.
  RejectToParentFollow,
};

/// The 6 closed structural facts the precedence table reads. These
/// are the only inputs to `decide_iteration_boundary`. Adding a fact
/// here changes the cardinality of the test corpus from 64 to 128
/// and requires a justification (the table is intentionally closed).
struct IterationBoundaryFacts {
  /// True iff the iteration has consumed at least one strict leaf
  /// without recovery edit (the iteration is mid-flight).
  bool startedStrictly = false;

  /// True iff the element accepts strictly at the current cursor.
  bool firstStrict = false;

  /// True iff the element accepts in recoverable mode at the current
  /// cursor.
  bool firstRecoverable = false;

  /// True iff the parent follow accepts strictly at the current
  /// cursor.
  bool followStrict = false;

  /// True iff the parent follow accepts in recoverable mode at the
  /// current cursor.
  bool followRecoverable = false;

  /// True iff the committed prefix forces the iteration to keep
  /// going (i.e. `StopCleanly` is inadmissible until the committed
  /// replay closes).
  bool committedPrefixImposes = false;

  [[nodiscard]] friend bool
  operator==(const IterationBoundaryFacts &a,
             const IterationBoundaryFacts &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<IterationBoundaryFacts>);
static_assert(sizeof(IterationBoundaryFacts) <= 8);

/// Computes the iteration boundary decision from the 6 facts.
///
/// The implementation is the closed precedence table:
///
///   1. `firstStrict`                        -> ContinueStrict
///   2. `followStrict && !committedImposes`  -> StopCleanly
///   3. `startedStrictly && !followStrict`   -> RepairStartedIteration
///   4. `firstRecoverable && !followStrict`  -> ContinueRecoverable
///   5. `!followStrict`                      -> Resync
///   6. otherwise                            -> RejectToParentFollow
///
/// The function is total and exclusive on the 64 combinations of the
/// 6 facts: every combination receives exactly one decision. This is
/// the property the exhaustive test pins. Some combinations are
/// declared impossible by construction (see
/// `kImpossibleIterationFactCombinations`); the function still
/// classifies them — the impossibility list is about which
/// combinations real grammars produce, not about which combinations
/// the table can handle.
///
/// Inputs the table does NOT read on purpose:
///   - resync budget: caller's responsibility; if `Resync` is
///     returned but the budget refuses to enumerate one, the caller
///     falls back to `StopCleanly` (or `RejectToParentFollow` when
///     the committed prefix imposes).
///   - candidate enumeration cost: the table decides which families
///     are allowed; the caller produces the actual candidates.
[[nodiscard]] constexpr IterationBoundaryDecision
decide_iteration_boundary(IterationBoundaryFacts facts) noexcept {
  if (facts.firstStrict) {
    return IterationBoundaryDecision::ContinueStrict;
  }
  if (facts.followStrict && !facts.committedPrefixImposes) {
    return IterationBoundaryDecision::StopCleanly;
  }
  if (!facts.followStrict) {
    if (facts.startedStrictly) {
      return IterationBoundaryDecision::RepairStartedIteration;
    }
    if (facts.firstRecoverable) {
      return IterationBoundaryDecision::ContinueRecoverable;
    }
    return IterationBoundaryDecision::Resync;
  }
  // followStrict && committedPrefixImposes: the strict parent follow
  // would block any local repair, but the committed prefix forbids
  // a clean stop. Defer to the parent.
  return IterationBoundaryDecision::RejectToParentFollow;
}

/// Returns a short stable identifier for the decision. Intended for
/// recovery traces, debug logs, and test failure messages — not for
/// any decision logic. The strings are stable across builds.
[[nodiscard]] constexpr const char *
iteration_boundary_decision_name(IterationBoundaryDecision decision) noexcept {
  switch (decision) {
  case IterationBoundaryDecision::StopCleanly:
    return "StopCleanly";
  case IterationBoundaryDecision::ContinueStrict:
    return "ContinueStrict";
  case IterationBoundaryDecision::ContinueRecoverable:
    return "ContinueRecoverable";
  case IterationBoundaryDecision::RepairStartedIteration:
    return "RepairStartedIteration";
  case IterationBoundaryDecision::Resync:
    return "Resync";
  case IterationBoundaryDecision::RejectToParentFollow:
    return "RejectToParentFollow";
  }
  return "Unknown";
}

/// Closed enumeration of the fact combinations declared impossible by
/// construction. The list is authoritative: combinations not in this
/// list MUST be reachable by real grammars and MUST receive an
/// `IterationBoundaryDecision`.
///
/// Each entry pairs the impossible facts with the structural reason.
struct ImpossibleIterationFactCombination {
  IterationBoundaryFacts facts;
  /// Short, stable identifier of the impossibility reason.
  const char *reason;
};

inline constexpr ImpossibleIterationFactCombination
    kImpossibleIterationFactCombinations[] = {
        // Two impossibilities, both expressing the design constraint
        // that the recoverable probes are extensions (supersets) of
        // their strict counterparts. A grammar where the strict probe
        // accepts but the recoverable probe does not would contradict
        // how the recoverable extension is constructed in Pegium. The
        // precedence function still classifies these inputs (giving
        // the "as-if reachable" decision), so an accidental violation
        // does not silently misroute control.
        {.facts = {.firstStrict = true, .firstRecoverable = false},
         .reason = "first_strict_implies_first_recoverable"},
        {.facts = {.followStrict = true, .followRecoverable = false},
         .reason = "follow_strict_implies_follow_recoverable"},
};

/// Returns true iff `facts` matches one of the
/// `kImpossibleIterationFactCombinations` patterns. A pattern matches
/// when every fact set to `true` in the pattern is also `true` in
/// `facts`. Other facts are unconstrained — the impossibility is
/// indexed by the pattern, not by the full 6-fact tuple.
[[nodiscard]] constexpr bool
is_impossible_iteration_fact_combination(IterationBoundaryFacts facts) noexcept {
  for (const auto &entry : kImpossibleIterationFactCombinations) {
    const bool matches =
        (!entry.facts.startedStrictly || facts.startedStrictly) &&
        (!entry.facts.firstStrict || facts.firstStrict) &&
        (!entry.facts.firstRecoverable || facts.firstRecoverable) &&
        (!entry.facts.followStrict || facts.followStrict) &&
        (!entry.facts.followRecoverable || facts.followRecoverable) &&
        (!entry.facts.committedPrefixImposes || facts.committedPrefixImposes);
    // The pattern intentionally encodes positive constraints only;
    // the impossibility is "this fact is true AND that fact is
    // false". Translate it explicitly here so the pattern stays
    // readable. The two patterns above are encoded with their
    // negative slot left at false, which means "AND that fact is
    // false in input too". Since matches above only checks positive
    // fields, we must additionally verify the negative slot:
    if (!matches) {
      continue;
    }
    if (entry.facts.firstStrict && !entry.facts.firstRecoverable &&
        facts.firstRecoverable) {
      continue;
    }
    if (entry.facts.followStrict && !entry.facts.followRecoverable &&
        facts.followRecoverable) {
      continue;
    }
    return true;
  }
  return false;
}

} // namespace pegium::parser::detail
