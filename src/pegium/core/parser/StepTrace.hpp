#pragma once

/// Lightweight step counters used by parser instrumentation hooks.

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>

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

#if defined(PEGIUM_ENABLE_STEP_TRACE)

#define PEGIUM_STEP_TRACE_INC(counter, ...)                                     \
  ::pegium::parser::detail::stepTraceInc(counter, ##__VA_ARGS__)
#define PEGIUM_STEP_TRACE_RESET() ::pegium::parser::detail::stepTraceReset()
#define PEGIUM_STEP_TRACE_DUMP_SUMMARY(...)                                     \
  ::pegium::parser::detail::stepTraceDumpSummary(__VA_ARGS__)

inline constexpr std::size_t
step_trace_index(StepCounter counter) noexcept {
  return static_cast<std::size_t>(counter);
}

inline const char *step_trace_name(StepCounter counter) noexcept {
  switch (counter) {
  case StepCounter::ParseContextMark:
    return "ParseContextMark";
  case StepCounter::ParseContextRewind:
    return "ParseContextRewind";
  case StepCounter::ParseContextEnter:
    return "ParseContextEnter";
  case StepCounter::ParseContextExit:
    return "ParseContextExit";
  case StepCounter::ParseContextLeaf:
    return "ParseContextLeaf";
  case StepCounter::ParseContextInsert:
    return "ParseContextInsert";
  case StepCounter::ParseContextInsertForced:
    return "ParseContextInsertForced";
  case StepCounter::ParseContextDelete:
    return "ParseContextDelete";
  case StepCounter::ParseContextReplace:
    return "ParseContextReplace";
  case StepCounter::ParsePhaseRuns:
    return "ParsePhaseRuns";
  case StepCounter::RecoveryPhaseRuns:
    return "RecoveryPhaseRuns";
  case StepCounter::ChoiceRecoverCalls:
    return "ChoiceRecoverCalls";
  case StepCounter::ChoiceFastAttempts:
    return "ChoiceFastAttempts";
  case StepCounter::ChoiceFastSuccess:
    return "ChoiceFastSuccess";
  case StepCounter::ChoiceFastFailures:
    return "ChoiceFastFailures";
  case StepCounter::ChoiceStrictPasses:
    return "ChoiceStrictPasses";
  case StepCounter::ChoiceEditablePasses:
    return "ChoiceEditablePasses";
  case StepCounter::GroupRecoverCalls:
    return "GroupRecoverCalls";
  case StepCounter::GroupFastAttempts:
    return "GroupFastAttempts";
  case StepCounter::GroupFastSuccess:
    return "GroupFastSuccess";
  case StepCounter::GroupFastFailures:
    return "GroupFastFailures";
  case StepCounter::GroupStrictPasses:
    return "GroupStrictPasses";
  case StepCounter::GroupEditablePasses:
    return "GroupEditablePasses";
  case StepCounter::UnorderedRecoverCalls:
    return "UnorderedRecoverCalls";
  case StepCounter::UnorderedFastAttempts:
    return "UnorderedFastAttempts";
  case StepCounter::UnorderedFastSuccess:
    return "UnorderedFastSuccess";
  case StepCounter::UnorderedFastFailures:
    return "UnorderedFastFailures";
  case StepCounter::UnorderedStrictPasses:
    return "UnorderedStrictPasses";
  case StepCounter::UnorderedEditablePasses:
    return "UnorderedEditablePasses";
  case StepCounter::RepetitionRecoverCalls:
    return "RepetitionRecoverCalls";
  case StepCounter::RepetitionFastAttempts:
    return "RepetitionFastAttempts";
  case StepCounter::RepetitionFastSuccess:
    return "RepetitionFastSuccess";
  case StepCounter::RepetitionFastFailures:
    return "RepetitionFastFailures";
  case StepCounter::Count:
    return "Count";
  }
  return "Unknown";
}

inline std::array<std::uint64_t, step_trace_index(StepCounter::Count)>
    stepTraceCounters{};

inline void stepTraceReset() noexcept {
  stepTraceCounters.fill(0);
}

inline void stepTraceInc(StepCounter counter,
                         std::uint64_t delta = 1) noexcept {
  stepTraceCounters[step_trace_index(counter)] += delta;
}

inline std::uint64_t stepTraceValue(StepCounter counter) noexcept {
  return stepTraceCounters[step_trace_index(counter)];
}

template <typename Stream>
inline void stepTraceDumpSummary(Stream &stream) noexcept {
  for (std::size_t i = 0; i < stepTraceCounters.size(); ++i) {
    const auto count = stepTraceCounters[i];
    if (count == 0) {
      continue;
    }
    const auto counter = static_cast<StepCounter>(i);
    stream << step_trace_name(counter) << ": " << count << '\n';
  }
}

#else

#define PEGIUM_STEP_TRACE_INC(...) ((void)0)
#define PEGIUM_STEP_TRACE_RESET() ((void)0)
#define PEGIUM_STEP_TRACE_DUMP_SUMMARY(...) ((void)0)

inline void stepTraceReset() noexcept {}
inline void stepTraceInc(StepCounter, std::uint64_t = 1) noexcept {}
template <typename... Args>
inline void stepTraceDumpSummary(Args &&...) noexcept {}

#endif

} // namespace pegium::parser::detail
