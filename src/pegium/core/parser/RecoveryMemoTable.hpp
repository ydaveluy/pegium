#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/Parser.hpp>

namespace pegium::parser::detail {

[[nodiscard]] constexpr std::uint64_t
mix_recovery_memo_signature(std::uint64_t base,
                            std::uint64_t extra) noexcept {
  extra ^= extra >> 33u;
  extra *= 14029467366897019727ull;
  extra ^= extra >> 29u;
  base ^= extra + 0x9e3779b97f4a7c15ull + (base << 6u) + (base >> 2u);
  return base;
}

enum class RecoveryMemoQueryKind : std::uint8_t {
  FastProbe,
  StartedWithoutEdits,
  EntryRecoverable,
  OrderedChoiceAttempt,
  RepetitionIteration,
  GroupMissingElementInsert,
};

struct RecoveryMemoKey {
  RecoveryMemoQueryKind queryKind = RecoveryMemoQueryKind::FastProbe;
  std::uint8_t purposeBits = 0;
  const void *ownerIdentity = nullptr;
  TextOffset cursorOffset = std::numeric_limits<TextOffset>::max();
  TextOffset furthestExploredOffset = std::numeric_limits<TextOffset>::max();
  std::uint64_t policySignature = 0;
  std::uint64_t activeRecoverySignature = 0;
};

struct OrderedChoiceAttemptMemoValue {
  std::size_t branchIndex = 0;
  TextOffset restartRetryCursorOffset = 0;
  TextOffset candidateCursorOffset = 0;
  TextOffset candidatePostSkipCursorOffset = 0;
  TextOffset candidateEditSpan = 0;
  TextOffset observedFurthestExploredOffset = 0;
  TextOffset candidateFirstEditOffset = std::numeric_limits<TextOffset>::max();
  const grammar::AbstractElement *candidateFirstEditElement = nullptr;
  std::uint32_t candidateEditCost = 0;
  std::uint32_t candidateEditCount = 0;
  std::uint8_t kind = 0;
  std::uint8_t restartReplayMode = 0;
  bool matched = false;
  bool hasDeleteEdit = false;
  bool startedWithoutEdits = false;
  bool branchHasStrictStartSignal = false;
};

struct RepetitionIterationMemoValue {
  TextOffset observedFurthestExploredOffset = 0;
  std::uint8_t kind = 0;
  bool matched = false;
  bool blockFrontier = false;
  bool allowInsert = false;
  bool allowDelete = false;
  bool allowDestructiveWindowContinuation = false;
  bool scopeLeadingTerminalInsert = false;
  bool allowExtendedDeleteScan = false;
  bool protectParseStartBoundary = false;
  bool protectLaterVisibleBoundary = false;
};

struct ProbeMemoValue {
  TextOffset observedFurthestExploredOffset = 0;
  bool result = false;
};

struct GroupMissingElementInsertMemoValue {
  TextOffset observedFurthestExploredOffset = 0;
  bool selectInsertReplay = false;
};

template <typename Value> struct RecoveryMemoEntry {
  RecoveryMemoKey key{};
  Value value{};
};

template <typename Value, std::size_t Size> class RecoveryMemoBank {
public:
  static_assert(std::has_single_bit(Size));
  static_assert(std::is_trivially_copyable_v<Value>);

  [[nodiscard]] bool tryGet(const RecoveryMemoKey &key,
                            Value &value) const noexcept {
    const auto &entry = _entries[index(key)];
    if (!same_key(entry.key, key)) {
      return false;
    }
    value = entry.value;
    return true;
  }

  void store(const RecoveryMemoKey &key, const Value &value) noexcept {
    _entries[index(key)] = {.key = key, .value = value};
  }

private:
  [[nodiscard]] static bool same_key(const RecoveryMemoKey &lhs,
                                     const RecoveryMemoKey &rhs) noexcept {
    return lhs.ownerIdentity != nullptr &&
           lhs.queryKind == rhs.queryKind &&
           lhs.purposeBits == rhs.purposeBits &&
           lhs.ownerIdentity == rhs.ownerIdentity &&
           lhs.cursorOffset == rhs.cursorOffset &&
           lhs.furthestExploredOffset == rhs.furthestExploredOffset &&
           lhs.policySignature == rhs.policySignature &&
           lhs.activeRecoverySignature == rhs.activeRecoverySignature;
  }

  [[nodiscard]] static std::size_t index(const RecoveryMemoKey &key) noexcept {
    auto mixed = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(key.ownerIdentity));
    mixed ^= static_cast<std::uint64_t>(key.queryKind) * 1099511628211ull;
    mixed ^= static_cast<std::uint64_t>(key.purposeBits) * 1469598103934665603ull;
    mixed ^= static_cast<std::uint64_t>(key.cursorOffset) * 11400714819323198485ull;
    mixed ^= static_cast<std::uint64_t>(key.furthestExploredOffset) *
             7809847782465536323ull;
    mixed ^= key.policySignature * 2946739916269131511ull;
    mixed ^= key.activeRecoverySignature * 1609587929392839161ull;
    mixed ^= mixed >> 33u;
    mixed *= 14029467366897019727ull;
    mixed ^= mixed >> 29u;
    return static_cast<std::size_t>(mixed & (Size - 1u));
  }

  std::array<RecoveryMemoEntry<Value>, Size> _entries{};
};

template <RecoveryMemoQueryKind Kind> struct RecoveryMemoTraits;

template <> struct RecoveryMemoTraits<RecoveryMemoQueryKind::FastProbe> {
  using Value = ProbeMemoValue;
  static constexpr std::size_t bankIndex = 0u;
};

template <>
struct RecoveryMemoTraits<RecoveryMemoQueryKind::StartedWithoutEdits> {
  using Value = ProbeMemoValue;
  static constexpr std::size_t bankIndex = 0u;
};

template <>
struct RecoveryMemoTraits<RecoveryMemoQueryKind::EntryRecoverable> {
  using Value = ProbeMemoValue;
  static constexpr std::size_t bankIndex = 0u;
};

template <>
struct RecoveryMemoTraits<RecoveryMemoQueryKind::OrderedChoiceAttempt> {
  using Value = OrderedChoiceAttemptMemoValue;
  static constexpr std::size_t bankIndex = 1u;
};

template <>
struct RecoveryMemoTraits<RecoveryMemoQueryKind::RepetitionIteration> {
  using Value = RepetitionIterationMemoValue;
  static constexpr std::size_t bankIndex = 2u;
};

template <>
struct RecoveryMemoTraits<RecoveryMemoQueryKind::GroupMissingElementInsert> {
  using Value = GroupMissingElementInsertMemoValue;
  static constexpr std::size_t bankIndex = 3u;
};

class RecoveryMemoTable {
public:
  static constexpr TextOffset kNoFurthestExploredOffset =
      std::numeric_limits<TextOffset>::max();

  template <RecoveryMemoQueryKind Kind>
  using ValueFor = typename RecoveryMemoTraits<Kind>::Value;

  template <RecoveryMemoQueryKind Kind>
  [[nodiscard]] bool tryGet(const RecoveryMemoKey &key,
                            ValueFor<Kind> &value) const noexcept {
    return bank<Kind>().tryGet(key, value);
  }

  template <RecoveryMemoQueryKind Kind>
  void store(const RecoveryMemoKey &key, const ValueFor<Kind> &value) noexcept {
    bank<Kind>().store(key, value);
  }

private:
  template <RecoveryMemoQueryKind Kind>
  [[nodiscard]] auto &bank() noexcept {
    if constexpr (RecoveryMemoTraits<Kind>::bankIndex == 0u) {
      return _probeBank;
    } else if constexpr (RecoveryMemoTraits<Kind>::bankIndex == 1u) {
      return _orderedChoiceBank;
    } else if constexpr (RecoveryMemoTraits<Kind>::bankIndex == 2u) {
      return _repetitionBank;
    } else {
      return _groupBank;
    }
  }

  template <RecoveryMemoQueryKind Kind>
  [[nodiscard]] const auto &bank() const noexcept {
    if constexpr (RecoveryMemoTraits<Kind>::bankIndex == 0u) {
      return _probeBank;
    } else if constexpr (RecoveryMemoTraits<Kind>::bankIndex == 1u) {
      return _orderedChoiceBank;
    } else if constexpr (RecoveryMemoTraits<Kind>::bankIndex == 2u) {
      return _repetitionBank;
    } else {
      return _groupBank;
    }
  }

  RecoveryMemoBank<ProbeMemoValue, 16384u> _probeBank{};
  RecoveryMemoBank<OrderedChoiceAttemptMemoValue, 16384u>
      _orderedChoiceBank{};
  RecoveryMemoBank<RepetitionIterationMemoValue, 16384u> _repetitionBank{};
  RecoveryMemoBank<GroupMissingElementInsertMemoValue, 16384u> _groupBank{};
};

static_assert(std::is_trivially_copyable_v<RecoveryMemoKey>);
static_assert(std::is_trivially_copyable_v<ProbeMemoValue>);
static_assert(std::is_trivially_copyable_v<OrderedChoiceAttemptMemoValue>);
static_assert(std::is_trivially_copyable_v<RepetitionIterationMemoValue>);
static_assert(std::is_trivially_copyable_v<GroupMissingElementInsertMemoValue>);

} // namespace pegium::parser::detail
