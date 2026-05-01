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
  RestartReplay,
};

enum class RestartReplayMode : std::uint8_t {
  Editable,
  NoEdit,
};

struct ChoiceAttempt {
  std::size_t branchIndex = 0;
  ChoiceAttemptKind kind = ChoiceAttemptKind::None;
  /// Legacy candidate, retained because replay reads its mutable
  /// state (`cursorOffset`, `editSpan`, `reachedEof`) and the
  /// dispatch's non-decision sites still consult it. The
  /// decision-relevant projection is mirrored on `envelope` and is
  /// the single channel the family-redundancy filter and the central
  /// `RecoveryKey` ranking read.
  EditableRecoveryCandidate recovery{};
  /// Closed-vocabulary view consumed by admission, family-redundancy
  /// and ranking. Built from `recovery` via `to_candidate_envelope`
  /// at every construction site so the two stay in sync; the
  /// dispatch reads the envelope for decisions and never the legacy
  /// candidate for those.
  CandidateEnvelope envelope{};
  TextOffset restartRetryCursorOffset = 0;
  RestartReplayMode restartReplayMode = RestartReplayMode::Editable;
  bool startedWithoutEdits = false;
  bool branchHasStrictStartSignal = false;
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
  bool allowDeleteRetry = false;
  bool allowExtendedDeleteScan = false;
  bool skipAfterDelete = false;
  bool allowDestructiveWindowContinuation = false;
  bool allowLeadingTerminalInsertScope = false;
  bool allowProvisionalFuzzyReplace = false;
  TextOffset provisionalFuzzyReplaceAnchorOffset = 0;
  bool inRecoveryPhase = false;
  bool hadEdits = false;
  bool insideEditWindow = false;
  bool completedWindowContinuation = false;

  [[nodiscard]] friend bool
  operator==(const RecoveryPolicyFingerprint &a,
             const RecoveryPolicyFingerprint &b) noexcept {
    return a.followProbeFn == b.followProbeFn &&
           a.followProbeData == b.followProbeData &&
           a.recoverableFollowProbeFn == b.recoverableFollowProbeFn &&
           a.recoverableFollowProbeData == b.recoverableFollowProbeData &&
           a.recoverableFollowConsumesVisibleProbeFn ==
               b.recoverableFollowConsumesVisibleProbeFn &&
           a.recoverableFollowConsumesVisibleProbeData ==
               b.recoverableFollowConsumesVisibleProbeData &&
           a.remainingEditBudget == b.remainingEditBudget &&
           a.consecutiveDeletes == b.consecutiveDeletes &&
           a.editFloorOffset == b.editFloorOffset &&
           a.allowInsert == b.allowInsert && a.allowDelete == b.allowDelete &&
           a.allowDeleteRetry == b.allowDeleteRetry &&
           a.allowExtendedDeleteScan == b.allowExtendedDeleteScan &&
           a.skipAfterDelete == b.skipAfterDelete &&
           a.allowDestructiveWindowContinuation ==
               b.allowDestructiveWindowContinuation &&
           a.allowLeadingTerminalInsertScope ==
               b.allowLeadingTerminalInsertScope &&
           a.allowProvisionalFuzzyReplace == b.allowProvisionalFuzzyReplace &&
           a.provisionalFuzzyReplaceAnchorOffset ==
               b.provisionalFuzzyReplaceAnchorOffset &&
           a.inRecoveryPhase == b.inRecoveryPhase && a.hadEdits == b.hadEdits &&
           a.insideEditWindow == b.insideEditWindow &&
           a.completedWindowContinuation == b.completedWindowContinuation;
  }
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

  ChoiceRecoverCache() : _entries(std::make_unique<std::array<Entry, kCapacity>>()) {}

  void reset() noexcept {
    for (auto &entry : *_entries) {
      entry.occupied = false;
    }
    _hits = 0;
    _misses = 0;
  }

  /// Toggles the cache off without dropping its storage. When disabled,
  /// `tryGet` always returns `nullptr` and the caller is forced to recompute
  /// every attempt. Stores still succeed (they are harmless) so re-enabling
  /// the cache does not require a warm-up pass. Used by the cache-neutrality
  /// test harness.
  void setDisabled(bool disabled) noexcept { _disabled = disabled; }

  [[nodiscard]] bool isDisabled() const noexcept { return _disabled; }

  [[nodiscard]] const ChoiceAttempt *
  tryGet(const ChoiceRecoverCacheKey &key) noexcept {
    if (_disabled) {
      ++_misses;
      return nullptr;
    }
    const auto index = slot(key);
    const auto &entry = (*_entries)[index];
    if (!entry.occupied || !keys_equal(entry.key, key)) {
      ++_misses;
      return nullptr;
    }
    ++_hits;
    return &entry.value;
  }

  void store(const ChoiceRecoverCacheKey &key,
             const ChoiceAttempt &value) noexcept {
    const auto index = slot(key);
    auto &entry = (*_entries)[index];
    entry.key = key;
    entry.value = value;
    entry.occupied = true;
  }

  [[nodiscard]] std::uint64_t hits() const noexcept { return _hits; }
  [[nodiscard]] std::uint64_t misses() const noexcept { return _misses; }

private:
  struct Entry {
    ChoiceRecoverCacheKey key{};
    ChoiceAttempt value{};
    bool occupied = false;
  };

  static std::size_t slot(const ChoiceRecoverCacheKey &key) noexcept {
    auto h = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(key.choice));
    h ^= static_cast<std::uint64_t>(key.cursorOffset) * 0x9E3779B97F4A7C15ULL;
    h ^= static_cast<std::uint64_t>(key.maxCursorOffset) *
         0x3C79AC492BA7B653ULL;
    h ^= static_cast<std::uint64_t>(key.furthestVisibleLeafCount) *
         0x6A5D39EAE116586DULL;
    h ^= static_cast<std::uint64_t>(key.currentVisibleLeafCount) *
         0x3A1F5B8D2C7E9011ULL;
    h ^= mix_policy(key.policy);
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return static_cast<std::size_t>(h) & (kCapacity - 1);
  }

  static std::uint64_t
  mix_policy(const RecoveryPolicyFingerprint &policy) noexcept {
    auto h = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(policy.followProbeFn));
    h ^= static_cast<std::uint64_t>(
             reinterpret_cast<std::uintptr_t>(policy.followProbeData)) *
         0x7F4A7C15A24BAED4ULL;
    h ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
             policy.recoverableFollowProbeFn)) *
         0xBF58476D1CE4E5B9ULL;
    h ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
             policy.recoverableFollowProbeData)) *
         0x165667B19E3779F9ULL;
    h ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
             policy.recoverableFollowConsumesVisibleProbeFn)) *
         0x9E3779B185EBCA87ULL;
    h ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(
             policy.recoverableFollowConsumesVisibleProbeData)) *
         0xC2B2AE3D27D4EB2FULL;
    h ^= static_cast<std::uint64_t>(policy.remainingEditBudget) *
         0xC2B2AE3D27D4EB4FULL;
    h ^= static_cast<std::uint64_t>(policy.consecutiveDeletes) *
         0x94D049BB133111EBULL;
    h ^= static_cast<std::uint64_t>(policy.editFloorOffset) *
         0x85EBCA77C2B2AE63ULL;
    h ^= static_cast<std::uint64_t>(
             policy.provisionalFuzzyReplaceAnchorOffset) *
         0x27D4EB2F165667C5ULL;
    const auto bools =
        (static_cast<std::uint32_t>(policy.allowInsert) << 0) |
        (static_cast<std::uint32_t>(policy.allowDelete) << 1) |
        (static_cast<std::uint32_t>(policy.allowDeleteRetry) << 2) |
        (static_cast<std::uint32_t>(policy.allowExtendedDeleteScan) << 3) |
        (static_cast<std::uint32_t>(policy.skipAfterDelete) << 4) |
        (static_cast<std::uint32_t>(policy.allowDestructiveWindowContinuation)
         << 5) |
        (static_cast<std::uint32_t>(policy.allowLeadingTerminalInsertScope)
         << 6) |
        (static_cast<std::uint32_t>(policy.allowProvisionalFuzzyReplace) << 7) |
        (static_cast<std::uint32_t>(policy.inRecoveryPhase) << 8) |
        (static_cast<std::uint32_t>(policy.hadEdits) << 9) |
        (static_cast<std::uint32_t>(policy.insideEditWindow) << 10) |
        (static_cast<std::uint32_t>(policy.completedWindowContinuation) << 11);
    h ^= static_cast<std::uint64_t>(bools) * 0xD1B54A32D192ED03ULL;
    return h;
  }

  static bool keys_equal(const ChoiceRecoverCacheKey &a,
                         const ChoiceRecoverCacheKey &b) noexcept {
    return a.choice == b.choice && a.cursorOffset == b.cursorOffset &&
           a.maxCursorOffset == b.maxCursorOffset &&
           a.furthestVisibleLeafCount == b.furthestVisibleLeafCount &&
           a.currentVisibleLeafCount == b.currentVisibleLeafCount &&
           a.policy == b.policy;
  }

  std::unique_ptr<std::array<Entry, kCapacity>> _entries;
  std::uint64_t _hits = 0;
  std::uint64_t _misses = 0;
  bool _disabled = false;
};

} // namespace pegium::parser::detail
