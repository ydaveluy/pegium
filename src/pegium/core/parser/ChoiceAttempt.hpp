#pragma once

/// Shared types for OrderedChoice recovery attempts and its memoization cache.
///
/// The cache captures the outcome of `OrderedChoice::evaluate_choice_attempts`
/// so that repeated recovery descents at the same `(choice, cursor, policy)`
/// key skip the whole branch-enumeration phase and jump straight to
/// `replay_choice_attempt`.
///
/// Cache neutrality: the cache is only a pure optimization.
/// Disabling it via `ChoiceRecoverCache::setDisabled(true)` must never
/// change the chosen recovery candidate. This requires every recovery
/// policy axis that can influence branch enumeration to participate
/// in `ChoiceRecoverCacheKey`. `RecoveryPolicyFingerprint` centralises
/// those axes into a single typed struct so the key cannot drift out
/// of sync with the policy it memoizes.

#include <array>
#include <cstdint>
#include <memory>

#include <pegium/core/parser/CandidateEnvelope.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/utils/TextUtils.hpp>

namespace pegium::parser::detail {

enum class ChoiceAttemptKind : std::uint8_t {
  None,
  NoEditReplay,
  Editable,
};

struct ChoiceAttempt {
  std::size_t branchIndex = 0;
  ChoiceAttemptKind kind = ChoiceAttemptKind::None;
  /// Closed-vocabulary view consumed by admission, family-redundancy
  /// and ranking. Built via `to_candidate_envelope` at every construction
  /// site; the dispatch reads the envelope for decisions.
  CandidateEnvelope envelope{};
  /// Post-evaluation cumulative cursors. These monotonic side effects are
  /// produced by exploring all branches, but replaying the single winner does
  /// not reproduce them. Cache hits must reapply these values to keep the
  /// downstream pipeline observing identical progress.
  TextOffset postEvalMaxCursorOffset = 0;
  TextOffset postEvalFurthestFailureOffset = 0;
  std::uint32_t postEvalFurthestVisibleLeafCount = 0;
};

/// Closed fingerprint of every recovery policy axis that can change the
/// evaluation outcome of an `OrderedChoice` branch enumeration.
///
/// Policy changes that occur mid-recovery (e.g. a scope that flips
/// `allowDelete`, a window that disables `allowLeadingTerminalInsertScope`,
/// a budget that drops `remainingEditBudget` below what an edit needs) must
/// force a cache miss, not silently reuse a cached winner computed under a
/// different policy. The fingerprint is the single source of truth for what
/// "same policy" means for the cache; adding a new policy axis to the
/// recovery context must also add it here, and failing to do so is a cache
/// neutrality bug.
///
/// Probe identities include both the function pointer and its opaque data.
/// Two probes that happen to share the same data pointer but resolve through
/// different functions are genuinely different observations; including both
/// closes that collision hole.
struct RecoveryPolicyFingerprint {
  using FollowProbeFnPtr = const void *;

  FollowProbeFnPtr followProbeFn = nullptr;
  const void *followProbeData = nullptr;
  FollowProbeFnPtr recoverableFollowProbeFn = nullptr;
  const void *recoverableFollowProbeData = nullptr;
  FollowProbeFnPtr recoverableFollowConsumesVisibleProbeFn = nullptr;
  const void *recoverableFollowConsumesVisibleProbeData = nullptr;
  std::uint32_t remainingEditBudget = 0;
  std::uint32_t consecutiveDeletes = 0;
  TextOffset editFloorOffset = 0;
  bool allowInsert = false;
  bool allowDelete = false;
  bool skipAfterDelete = false;
  bool allowDestructiveWindowContinuation = false;
  bool allowLeadingTerminalInsertScope = false;
  bool inRecoveryPhase = false;
  bool hadEdits = false;
  bool insideEditWindow = false;
  bool completedWindowContinuation = false;
  // Read by Repetition / UnorderedGroup / InfixRule recovery decisions
  // (`Repetition.hpp:242`, `UnorderedGroup.hpp:102`, `InfixRule.hpp:670`).
  // A choice attempt cached at one value can replay a wrong winner if the
  // bit flips between visits.
  bool frontierBlocked = false;
  // Gates `insertSynthetic`/`deleteOneCodepoint`/`replaceLeaf` at every
  // edit site (`ParseContext.cpp:48,89,133,213`). Flipped by
  // `EditStateGuard` (e.g. `withEditTrackingDisabled` in `InfixRule`).
  bool trackEditState = false;
  // Index into committed-recovery-edits replay; controls whether edits
  // are admissible at the current cursor (`ParseContext.cpp:53,97,141`).
  std::uint32_t committedRecoveryEditIndex = 0;
  // Per-attempt edit budgets. Mutated by `ExtendedDeleteScanBudgetScope`
  // (`RecoveryUtils.hpp:54-55`); affects which candidates are admissible.
  std::uint32_t remainingEditCount = 0;
  std::uint32_t remainingConsecutiveDeletes = 0;

  // Defaulted: every field participates in the cache key, so adding a
  // new field automatically extends the comparison. Manually written
  // comparisons silently drop a new field — a cache-poisoning footgun.
  [[nodiscard]] friend bool
  operator==(const RecoveryPolicyFingerprint &a,
             const RecoveryPolicyFingerprint &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<RecoveryPolicyFingerprint>);

struct ChoiceRecoverCacheKey {
  const void *choice = nullptr;
  TextOffset cursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  std::uint32_t furthestVisibleLeafCount = 0;
  std::uint32_t currentVisibleLeafCount = 0;
  RecoveryPolicyFingerprint policy{};
};

class ChoiceRecoverCache {
public:
  // Recovery-heavy malformed clusters can revisit the same ordered choices many
  // times within one parse window. A slightly larger cache avoids direct-mapped
  // thrash there without affecting nominal strict parses. The storage is heap-
  // allocated so that RecoveryContext instances stay small on the recursion
  // stack.
  static constexpr std::size_t kCapacity = 8192;
  static_assert((kCapacity & (kCapacity - 1)) == 0,
                "ChoiceRecoverCache capacity must be a power of two.");

  ChoiceRecoverCache();

  /// Toggles the cache off without dropping its storage. When disabled,
  /// `tryGet` always returns `nullptr` and the caller is forced to recompute
  /// every attempt. Stores still succeed (they are harmless) so re-enabling
  /// the cache does not require a warm-up pass. Used by the cache-neutrality
  /// test harness.
  void setDisabled(bool disabled) noexcept { _disabled = disabled; }

  [[nodiscard]] bool isDisabled() const noexcept { return _disabled; }

  [[nodiscard]] const ChoiceAttempt *
  tryGet(const ChoiceRecoverCacheKey &key) noexcept;

  void store(const ChoiceRecoverCacheKey &key,
             const ChoiceAttempt &value) noexcept;

  [[nodiscard]] std::uint64_t hits() const noexcept { return _hits; }
  [[nodiscard]] std::uint64_t misses() const noexcept { return _misses; }

private:
  struct Entry {
    ChoiceRecoverCacheKey key{};
    ChoiceAttempt value{};
    bool valid = false;
  };

  std::unique_ptr<std::array<Entry, kCapacity>> _entries;
  std::uint64_t _hits = 0;
  std::uint64_t _misses = 0;
  bool _disabled = false;
};

} // namespace pegium::parser::detail
