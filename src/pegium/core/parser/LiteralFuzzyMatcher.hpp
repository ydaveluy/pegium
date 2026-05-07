#pragma once

/// Fuzzy literal matching utilities used during recovery.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <pegium/core/parser/RecoveryCost.hpp>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pegium::parser::detail {

struct LiteralFuzzyCandidate {
  std::size_t consumed = 0;
  std::uint32_t distance = 0;
  std::uint32_t operationCount = 0;
  std::uint32_t rawWeightedCost = 0;
  RecoveryCost cost{};
  std::uint32_t insertionCount = 0;
  std::uint32_t deletionCount = 0;
  std::uint32_t substitutionCount = 0;
  std::uint32_t transpositionCount = 0;
};

using LiteralFuzzyCandidates = std::vector<LiteralFuzzyCandidate>;

/// Direct-mapped cache for `find_literal_fuzzy_candidates`.
///
/// Memoizes the Levenshtein-DP keyed on the (literal storage, input span,
/// case-sensitivity) identity tuple. Both pointers are stable for the
/// duration of a single parse: literal storage lives in the grammar, and the
/// input view points into the immutable text snapshot. The cache must
/// therefore live no longer than one parse — embed it in the context that
/// owns the parse, do not promote it to a thread-local or global.
///
/// Storage layout: each slot's metadata + result span is a POD `Entry`.
/// The result data is heap-allocated separately (raw `LiteralFuzzyCandidate`
/// array, freed in the cache destructor for entries whose `generation > 0`).
/// The slot array itself is allocated raw via `::operator new`; only the
/// `generation` field of each entry is initialised to 0 in the constructor.
/// This skips the ~`kSlotCount * sizeof(Entry)` zero-init that
/// value-initialised arrays would otherwise force on every
/// `RecoveryContext` construction — that memset accounted for ~3% of Ir on
/// the DomainModel keyword fuzz once the slot count was scaled up to
/// capture the full working set.
class LiteralFuzzyCandidatesCache {
public:
  struct Entry {
    const char *literalData;
    std::size_t literalSize;
    const char *inputData;
    std::size_t inputSize;
    LiteralFuzzyCandidate *resultData;
    std::uint32_t resultSize;
    std::uint32_t generation;
    bool caseSensitive;
  };
  static_assert(std::is_trivially_destructible_v<Entry>);

  /// Sized for the heaviest fuzzy workload in the suite (DomainModel
  /// keyword fuzz, ~5M lookups). Hit-rate plateau measured on that
  /// benchmark: 256→86.0% · 1024→92.6% · 2048→94.1% · 4096→94.9%. With
  /// the generation-counter trick below, scaling to 4096 carries no
  /// per-Entry zero-init cost.
  static constexpr std::size_t kSlotCount = 4096;
  static_assert((kSlotCount & (kSlotCount - 1)) == 0,
                "kSlotCount must be a power of two for masked indexing.");

  LiteralFuzzyCandidatesCache();
  ~LiteralFuzzyCandidatesCache() noexcept;

  LiteralFuzzyCandidatesCache(const LiteralFuzzyCandidatesCache &) = delete;
  LiteralFuzzyCandidatesCache(LiteralFuzzyCandidatesCache &&) = delete;
  LiteralFuzzyCandidatesCache &
  operator=(const LiteralFuzzyCandidatesCache &) = delete;
  LiteralFuzzyCandidatesCache &
  operator=(LiteralFuzzyCandidatesCache &&) = delete;

  [[nodiscard]] Entry *slotAt(std::size_t index) noexcept {
    return &(*_entries)[index];
  }

private:
  // Raw owning storage. Default-constructed `std::array<Entry, N>` would
  // value-initialise every Entry; we allocate uninitialised memory and
  // only zero the `generation` field.
  std::unique_ptr<std::array<Entry, kSlotCount>> _entries;
};

[[nodiscard]] LiteralFuzzyCandidates
find_literal_fuzzy_candidates(std::string_view literal, std::string_view input,
                              bool caseSensitive,
                              LiteralFuzzyCandidatesCache *cache = nullptr) noexcept;

/// Like `find_literal_fuzzy_candidates` but returns a non-owning view into
/// the cache slot. Used on the recovery hot path so a cache hit is
/// allocation-free; the view stays valid until the next call that targets
/// the same slot (which is the caller's `RecoveryContext` cache anyway).
/// On miss the result is computed, written into the slot, and the
/// returned view points at the freshly populated slot storage.
[[nodiscard]] std::span<const LiteralFuzzyCandidate>
find_literal_fuzzy_candidates_view(std::string_view literal,
                                   std::string_view input, bool caseSensitive,
                                   LiteralFuzzyCandidatesCache &cache) noexcept;

[[nodiscard]] std::optional<LiteralFuzzyCandidate>
find_best_literal_fuzzy_candidate(std::string_view literal,
                                  std::string_view input,
                                  bool caseSensitive) noexcept;

/// Strict-pass single-edit detector. Returns true iff `literal` matches a
/// prefix of `window` of length L = N (substitute / transpose), L = N+1
/// (insert in window) or L = N-1 (delete from window) with exactly one
/// edit operation. Equivalent to the strict-path DP probe
/// (`find_best_literal_fuzzy_candidate(...).distance == 1 && |consumed-N|
/// <= 1`) but runs in O(N) without the Levenshtein DP — keeps the
/// failure-tracking strict pass free of recovery DP cost.
[[nodiscard]] bool
literal_has_single_edit_strict_match(std::string_view literal,
                                     std::string_view window,
                                     bool caseSensitive) noexcept;

} // namespace pegium::parser::detail
