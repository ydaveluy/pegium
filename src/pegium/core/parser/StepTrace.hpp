#pragma once

/// Lightweight step counters used by parser instrumentation hooks.

#include <cstddef>
#include <cstdint>

namespace pegium::parser::detail {

enum class StepCounter : std::size_t {

  ParseContextMark,
  ParseContextRewind,
  ParseContextEnter,
  ParseContextExit,
  ParseContextLeaf,
  ParseContextInsert,
  ParseContextInsertForced,
  ParseContextDelete,
  ParseContextReplace,
  ParsePhaseRuns,
  RecoveryPhaseRuns,
  ChoiceRecoverCalls,
  ChoiceFastAttempts,
  ChoiceFastSuccess,
  ChoiceFastFailures,
  ChoiceStrictPasses,
  ChoiceEditablePasses,
  GroupRecoverCalls,
  GroupFastAttempts,
  GroupFastSuccess,
  GroupFastFailures,
  GroupStrictPasses,
  GroupEditablePasses,
  UnorderedRecoverCalls,
  UnorderedFastAttempts,
  UnorderedFastSuccess,
  UnorderedFastFailures,
  UnorderedStrictPasses,
  UnorderedEditablePasses,
  RepetitionRecoverCalls,
  RepetitionFastAttempts,
  RepetitionFastSuccess,
  RepetitionFastFailures,
  Count
};

inline void stepTraceReset() noexcept {}
inline void stepTraceInc(StepCounter, std::uint64_t = 1) noexcept {}
template <typename... Args>
inline void stepTraceDumpSummary(Args &&...) noexcept {}

} // namespace pegium::parser::detail
