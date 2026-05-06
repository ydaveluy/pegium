#include <pegium/core/parser/ChoiceAttempt.hpp>

#include <limits>
#include <new>
#include <type_traits>

namespace pegium::parser::detail {

namespace {

[[nodiscard]] std::uint64_t
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
  h ^= static_cast<std::uint64_t>(policy.provisionalFuzzyReplaceAnchorOffset) *
       0x27D4EB2F165667C5ULL;
  const auto bools =
      (static_cast<std::uint32_t>(policy.allowInsert) << 0) |
      (static_cast<std::uint32_t>(policy.allowDelete) << 1) |
      (static_cast<std::uint32_t>(policy.allowDeleteRetry) << 2) |
      (static_cast<std::uint32_t>(policy.allowExtendedDeleteScan) << 3) |
      (static_cast<std::uint32_t>(policy.skipAfterDelete) << 4) |
      (static_cast<std::uint32_t>(policy.allowDestructiveWindowContinuation)
       << 5) |
      (static_cast<std::uint32_t>(policy.allowLeadingTerminalInsertScope) << 6) |
      (static_cast<std::uint32_t>(policy.allowProvisionalFuzzyReplace) << 7) |
      (static_cast<std::uint32_t>(policy.inRecoveryPhase) << 8) |
      (static_cast<std::uint32_t>(policy.hadEdits) << 9) |
      (static_cast<std::uint32_t>(policy.insideEditWindow) << 10) |
      (static_cast<std::uint32_t>(policy.completedWindowContinuation) << 11);
  h ^= static_cast<std::uint64_t>(bools) * 0xD1B54A32D192ED03ULL;
  return h;
}

[[nodiscard]] std::size_t slot(const ChoiceRecoverCacheKey &key) noexcept {
  auto h = static_cast<std::uint64_t>(
      reinterpret_cast<std::uintptr_t>(key.choice));
  h ^= static_cast<std::uint64_t>(key.cursorOffset) * 0x9E3779B97F4A7C15ULL;
  h ^= static_cast<std::uint64_t>(key.maxCursorOffset) * 0x3C79AC492BA7B653ULL;
  h ^= static_cast<std::uint64_t>(key.furthestVisibleLeafCount) *
       0x6A5D39EAE116586DULL;
  h ^= static_cast<std::uint64_t>(key.currentVisibleLeafCount) *
       0x3A1F5B8D2C7E9011ULL;
  h ^= mix_policy(key.policy);
  h ^= h >> 33;
  h *= 0xFF51AFD7ED558CCDULL;
  h ^= h >> 33;
  return static_cast<std::size_t>(h) & (ChoiceRecoverCache::kCapacity - 1);
}

[[nodiscard]] bool keys_equal(const ChoiceRecoverCacheKey &a,
                              const ChoiceRecoverCacheKey &b) noexcept {
  return a.choice == b.choice && a.cursorOffset == b.cursorOffset &&
         a.maxCursorOffset == b.maxCursorOffset &&
         a.furthestVisibleLeafCount == b.furthestVisibleLeafCount &&
         a.currentVisibleLeafCount == b.currentVisibleLeafCount &&
         a.policy == b.policy;
}

} // namespace

ChoiceRecoverCache::ChoiceRecoverCache() {
  // Allocate uninitialised storage and only initialise the generation
  // field of each entry. Keys and values are not constructed; tryGet
  // rejects every slot whose generation != _currentGeneration before
  // reading any other field, so the uninit key/value bytes are never
  // observed in production. store() overwrites both before raising
  // the generation, fully constructing the entry. Saves ~1.4 MB of
  // memset per cache construction (~12% of total Ir on recovery-heavy
  // workloads in callgrind).
  static_assert(std::is_trivially_destructible_v<Entry>);
  void *const raw = ::operator new(sizeof(std::array<Entry, kCapacity>));
  _entries.reset(static_cast<std::array<Entry, kCapacity> *>(raw));
  for (auto &entry : *_entries) {
    entry.generation = 0;
  }
}

void ChoiceRecoverCache::reset() noexcept {
  // Bump the generation instead of walking 8192 entries. Stale entries
  // are filtered by the generation check in `tryGet`. On the rare
  // wraparound (every 2^32 resets), do one real walk to clear all
  // generations back to 0 so the next bump (1) is unambiguous.
  if (_currentGeneration == std::numeric_limits<std::uint32_t>::max())
      [[unlikely]] {
    for (auto &entry : *_entries) {
      entry.generation = 0;
    }
    _currentGeneration = 1;
  } else {
    ++_currentGeneration;
  }
  _hits = 0;
  _misses = 0;
}

const ChoiceAttempt *
ChoiceRecoverCache::tryGet(const ChoiceRecoverCacheKey &key) noexcept {
  if (_disabled) {
    ++_misses;
    return nullptr;
  }
  const auto index = slot(key);
  const auto &entry = (*_entries)[index];
  if (entry.generation != _currentGeneration || !keys_equal(entry.key, key)) {
    ++_misses;
    return nullptr;
  }
  ++_hits;
  return &entry.value;
}

void ChoiceRecoverCache::store(const ChoiceRecoverCacheKey &key,
                               const ChoiceAttempt &value) noexcept {
  const auto index = slot(key);
  auto &entry = (*_entries)[index];
  entry.key = key;
  entry.value = value;
  entry.generation = _currentGeneration;
}

} // namespace pegium::parser::detail
