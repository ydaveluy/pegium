#include <pegium/core/parser/ChoiceAttempt.hpp>

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
  // Every fingerprint axis that participates in the defaulted `operator==`
  // also participates in the hash. Skipping any axis here merely degrades
  // hit rate (collisions are filtered by the equality check), but keeping
  // hash and equality in lock-step makes adding a new axis a one-line edit
  // instead of a future-debugging session.
  h ^= static_cast<std::uint64_t>(policy.remainingEditBudget) *
       0xC2B2AE3D27D4EB4FULL;
  h ^= static_cast<std::uint64_t>(policy.consecutiveDeletes) *
       0x94D049BB133111EBULL;
  h ^= static_cast<std::uint64_t>(policy.editFloorOffset) *
       0x85EBCA77C2B2AE63ULL;
  h ^= static_cast<std::uint64_t>(policy.committedRecoveryEditIndex) *
       0xA29F0F39DD9E12C5ULL;
  h ^= static_cast<std::uint64_t>(policy.remainingEditCount) *
       0x4F2162926E40C299ULL;
  h ^= static_cast<std::uint64_t>(policy.remainingConsecutiveDeletes) *
       0x6E8E7A2F8B1B5C1DULL;
  const auto bools =
      (static_cast<std::uint32_t>(policy.allowInsert) << 0) |
      (static_cast<std::uint32_t>(policy.allowDelete) << 1) |
      (static_cast<std::uint32_t>(policy.skipAfterDelete) << 2) |
      (static_cast<std::uint32_t>(policy.allowDestructiveWindowContinuation)
       << 3) |
      (static_cast<std::uint32_t>(policy.allowLeadingTerminalInsertScope) << 4) |
      (static_cast<std::uint32_t>(policy.inRecoveryPhase) << 5) |
      (static_cast<std::uint32_t>(policy.hadEdits) << 6) |
      (static_cast<std::uint32_t>(policy.insideEditWindow) << 7) |
      (static_cast<std::uint32_t>(policy.completedWindowContinuation) << 8) |
      (static_cast<std::uint32_t>(policy.frontierBlocked) << 9) |
      (static_cast<std::uint32_t>(policy.trackEditState) << 10);
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
  // Allocate uninitialised storage and only initialise the `valid` flag
  // of each entry. Keys and values are not constructed; tryGet rejects
  // every slot whose `valid` is false before reading any other field,
  // so the uninit key/value bytes are never observed in production.
  // store() overwrites both before raising `valid`, fully constructing
  // the entry. Saves ~1.4 MB of memset per cache construction (~12% of
  // total Ir on recovery-heavy workloads in callgrind).
  static_assert(std::is_trivially_destructible_v<Entry>);
  void *const raw = ::operator new(sizeof(std::array<Entry, kCapacity>));
  _entries.reset(static_cast<std::array<Entry, kCapacity> *>(raw));
  for (auto &entry : *_entries) {
    entry.valid = false;
  }
}

const ChoiceAttempt *
ChoiceRecoverCache::tryGet(const ChoiceRecoverCacheKey &key) noexcept {
  if (_disabled) {
    ++_misses;
    return nullptr;
  }
  const auto index = slot(key);
  const auto &entry = (*_entries)[index];
  if (!entry.valid || !keys_equal(entry.key, key)) {
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
  entry.valid = true;
}

} // namespace pegium::parser::detail
