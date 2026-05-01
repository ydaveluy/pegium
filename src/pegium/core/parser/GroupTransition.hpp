#pragma once

/// `GroupTransition`: closed enumeration of the sequence-level
/// transitions a `Group` recovery considers when a strict element
/// fails inside the sequence.
///
/// `Group` recovery is framed as a small machine of transitions:
///
///   - `KeepCurrent`           keep parsing the current element strictly
///   - `RepairCurrent`         locally repair the current element
///   - `SkipNullable`          skip a nullable current element
///   - `InsertMissingCurrent`  synthesise the missing current element
///   - `RepairTail`            repair the sequence tail past the current
///
/// The vocabulary here is closed; adding a value requires removing or
/// merging an existing one (density ceiling rule).
///
/// Strict-path discipline: this header is recovery-side. It must
/// never be constructed on the strict-only nominal path.

#include <cstdint>
#include <type_traits>

namespace pegium::parser::detail {

/// Closed list of `Group` transitions. The 5 values are exhaustive
/// and frozen by the density ceiling rule: a new transition requires
/// removing or merging an existing one.
enum class GroupTransition : std::uint8_t {
  /// Keep parsing the current sequence element strictly. No edit.
  KeepCurrent,
  /// Locally repair the current sequence element. The repair must
  /// not cross a strict parent follow.
  RepairCurrent,
  /// Skip the current element because it is nullable and has not
  /// consumed any visible leaf yet.
  SkipNullable,
  /// Synthesise the missing current element when the current has
  /// not started, the insertion is replayable, and the tail
  /// provides a continuation independent of the inserted element.
  InsertMissingCurrent,
  /// Repair the tail of the sequence (skip past the current to a
  /// recoverable suffix). The current must already be strictly
  /// acquired or nullable; this transition cannot serve as entry
  /// proof for the current.
  RepairTail,
};

/// Facts the closed legality predicates of `GroupTransition` consume.
/// Each transition reads only a subset; the full list lives here so
/// callers fill the bundle once. Adding a fact here must come with
/// its consumer (one of the legality predicates below) and a test
/// pinning the new dependency.
struct GroupTransitionLegalityFacts {
  /// True iff the current element accepts strictly at the cursor
  /// without any edit (no-edit success).
  bool currentMatchesStrict = false;

  /// True iff the current element's first set matches at the cursor
  /// (strict or recoverable). Required for `RepairCurrent`.
  bool currentEntrySignal = false;

  /// True iff the current element is grammatically nullable.
  bool currentNullable = false;

  /// True iff the current element has consumed at least one visible
  /// (non-trivia, non-synthetic) leaf. Disqualifies `SkipNullable`.
  bool currentVisibleLeafConsumed = false;

  /// True iff the current element has been strictly acquired
  /// (parsed to completion strictly). Required for `RepairTail`.
  bool currentStrictlyAcquired = false;

  /// True iff the parent's strict follow accepts at the cursor and
  /// would block any local repair in the current element.
  bool parentFollowStrict = false;

  /// True iff the tail of the sequence has a strict or recoverable
  /// entry signal that does not depend on the inserted current
  /// (when `InsertMissingCurrent` is being considered).
  bool tailEntrySignalIndependent = false;

  /// True iff the tail of the sequence is grammatically nullable
  /// from the cursor.
  bool tailNullable = false;

  /// True iff the candidate insertion plan for the current element
  /// is replayable (single bounded edit, deterministic).
  bool insertionReplayable = false;

  /// True iff the immediately preceding nullable sibling can claim visible
  /// source at the current cursor, either by starting strictly there or by a
  /// replayable recovery that consumes visible source beyond the cursor.
  /// `InsertMissingCurrent` must reject in this case; otherwise the
  /// following required element can be fabricated before source that
  /// belongs to the nullable sibling.
  bool previousNullableSiblingOwnsCursor = false;

  [[nodiscard]] friend bool
  operator==(const GroupTransitionLegalityFacts &a,
             const GroupTransitionLegalityFacts &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<GroupTransitionLegalityFacts>);
static_assert(sizeof(GroupTransitionLegalityFacts) <= 16);

/// Closed legality predicate for `KeepCurrent`. Reads only
/// `currentMatchesStrict`.
[[nodiscard]] constexpr bool
is_keep_current_legal(const GroupTransitionLegalityFacts &facts) noexcept {
  return facts.currentMatchesStrict;
}

/// Closed legality predicate for `RepairCurrent`. Reads
/// `currentEntrySignal` and `parentFollowStrict`.
[[nodiscard]] constexpr bool
is_repair_current_legal(const GroupTransitionLegalityFacts &facts) noexcept {
  return facts.currentEntrySignal && !facts.parentFollowStrict;
}

/// Closed legality predicate for `SkipNullable`. Reads
/// `currentNullable` and `currentVisibleLeafConsumed`.
[[nodiscard]] constexpr bool
is_skip_nullable_legal(const GroupTransitionLegalityFacts &facts) noexcept {
  return facts.currentNullable && !facts.currentVisibleLeafConsumed;
}

/// Closed legality predicate for `InsertMissingCurrent`. Reads
/// `currentVisibleLeafConsumed` (must be false), `insertionReplayable`,
/// `tailEntrySignalIndependent`, and
/// `previousNullableSiblingOwnsCursor` (must be false).
[[nodiscard]] constexpr bool
is_insert_missing_current_legal(
    const GroupTransitionLegalityFacts &facts) noexcept {
  return !facts.currentVisibleLeafConsumed && facts.insertionReplayable &&
         facts.tailEntrySignalIndependent &&
         !facts.previousNullableSiblingOwnsCursor;
}

/// Closed legality predicate for `RepairTail`. Reads
/// `currentStrictlyAcquired`.
///
/// `RepairTail` keeps the Current as parsed and re-attempts the tail.
/// It is admissible iff the Current actually committed something (cursor
/// advanced or edits accumulated — captured by `currentStrictlyAcquired`).
/// A nullable Current that matched ε produces an identical state to
/// `SkipNullable`, which is already attempted earlier in
/// `parse_elements_recovery`, so admitting `RepairTail` in that case
/// would re-try the tail at the same cursor with no new information.
[[nodiscard]] constexpr bool
is_repair_tail_legal(const GroupTransitionLegalityFacts &facts) noexcept {
  return facts.currentStrictlyAcquired;
}

/// Closed dispatch over the legality predicates. Returns true iff
/// the transition is admissible under the given facts. The function
/// is total over the closed `GroupTransition` enum; a future value
/// added without updating the dispatch is a compile-time error
/// (the switch has no default).
[[nodiscard]] constexpr bool
is_group_transition_legal(GroupTransition transition,
                          const GroupTransitionLegalityFacts &facts) noexcept {
  switch (transition) {
  case GroupTransition::KeepCurrent:
    return is_keep_current_legal(facts);
  case GroupTransition::RepairCurrent:
    return is_repair_current_legal(facts);
  case GroupTransition::SkipNullable:
    return is_skip_nullable_legal(facts);
  case GroupTransition::InsertMissingCurrent:
    return is_insert_missing_current_legal(facts);
  case GroupTransition::RepairTail:
    return is_repair_tail_legal(facts);
  }
  return false;
}

/// Returns a short stable identifier for the transition. Intended
/// for recovery traces, debug logs, and test failure messages.
[[nodiscard]] constexpr const char *
group_transition_name(GroupTransition transition) noexcept {
  switch (transition) {
  case GroupTransition::KeepCurrent:
    return "KeepCurrent";
  case GroupTransition::RepairCurrent:
    return "RepairCurrent";
  case GroupTransition::SkipNullable:
    return "SkipNullable";
  case GroupTransition::InsertMissingCurrent:
    return "InsertMissingCurrent";
  case GroupTransition::RepairTail:
    return "RepairTail";
  }
  return "Unknown";
}

} // namespace pegium::parser::detail
