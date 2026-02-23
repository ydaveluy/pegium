#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#if defined(PEGIUM_ENABLE_STEP_TRACE)
#include <iostream>
#include <string_view>
#endif

namespace pegium::parser::detail {

enum class StepCounter : std::size_t {
  ParseStateMark,
  ParseStateRewind,
  ParseStateEnter,
  ParseStateExit,
  ParseStateLeaf,
  RecoverStateMark,
  RecoverStateRewind,
  RecoverStateEnter,
  RecoverStateExit,
  RecoverStateLeaf,
  RecoverStateInsert,
  RecoverStateInsertForced,
  RecoverStateDelete,
  RecoverStateReplace,
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

struct StepTraceState {
  std::array<std::uint64_t, static_cast<std::size_t>(StepCounter::Count)>
      counters{};
};

inline thread_local StepTraceState g_stepTraceState{};

inline constexpr std::size_t stepCounterIndex(StepCounter counter) noexcept {
  return static_cast<std::size_t>(counter);
}

inline void stepTraceReset() noexcept { g_stepTraceState = {}; }

inline void stepTraceInc(StepCounter counter, std::uint64_t delta = 1) noexcept {
  g_stepTraceState.counters[stepCounterIndex(counter)] += delta;
}

inline std::uint64_t stepTraceGet(StepCounter counter) noexcept {
  return g_stepTraceState.counters[stepCounterIndex(counter)];
}

inline void stepTraceDumpSummary(std::string_view label, bool ret, bool recovered,
                                 std::size_t len,
                                 std::size_t inputSize) {
  std::cerr << "[step-trace] rule=" << label << " ret=" << ret
            << " recovered=" << recovered << " len=" << len << '/'
            << inputSize << '\n';
  std::cerr << "  parse_phase_runs=" << stepTraceGet(StepCounter::ParsePhaseRuns)
            << " recovery_phase_runs="
            << stepTraceGet(StepCounter::RecoveryPhaseRuns) << '\n';
  std::cerr << "  parse_state: mark=" << stepTraceGet(StepCounter::ParseStateMark)
            << " rewind=" << stepTraceGet(StepCounter::ParseStateRewind)
            << " enter=" << stepTraceGet(StepCounter::ParseStateEnter)
            << " exit=" << stepTraceGet(StepCounter::ParseStateExit)
            << " leaf=" << stepTraceGet(StepCounter::ParseStateLeaf) << '\n';
  std::cerr << "  recover_state: mark="
            << stepTraceGet(StepCounter::RecoverStateMark)
            << " rewind=" << stepTraceGet(StepCounter::RecoverStateRewind)
            << " enter=" << stepTraceGet(StepCounter::RecoverStateEnter)
            << " exit=" << stepTraceGet(StepCounter::RecoverStateExit)
            << " leaf=" << stepTraceGet(StepCounter::RecoverStateLeaf)
            << " insert=" << stepTraceGet(StepCounter::RecoverStateInsert)
            << " force_insert="
            << stepTraceGet(StepCounter::RecoverStateInsertForced)
            << " delete=" << stepTraceGet(StepCounter::RecoverStateDelete)
            << " replace=" << stepTraceGet(StepCounter::RecoverStateReplace)
            << '\n';
  std::cerr << "  choice rule: calls="
            << stepTraceGet(StepCounter::ChoiceRecoverCalls)
            << " fast_try=" << stepTraceGet(StepCounter::ChoiceFastAttempts)
            << " fast_ok=" << stepTraceGet(StepCounter::ChoiceFastSuccess)
            << " fast_fail=" << stepTraceGet(StepCounter::ChoiceFastFailures)
            << " strict=" << stepTraceGet(StepCounter::ChoiceStrictPasses)
            << " editable="
            << stepTraceGet(StepCounter::ChoiceEditablePasses) << '\n';
  std::cerr << "  group rule: calls="
            << stepTraceGet(StepCounter::GroupRecoverCalls)
            << " fast_try=" << stepTraceGet(StepCounter::GroupFastAttempts)
            << " fast_ok=" << stepTraceGet(StepCounter::GroupFastSuccess)
            << " fast_fail=" << stepTraceGet(StepCounter::GroupFastFailures)
            << " strict=" << stepTraceGet(StepCounter::GroupStrictPasses)
            << " editable="
            << stepTraceGet(StepCounter::GroupEditablePasses) << '\n';
  std::cerr << "  unordered rule: calls="
            << stepTraceGet(StepCounter::UnorderedRecoverCalls)
            << " fast_try=" << stepTraceGet(StepCounter::UnorderedFastAttempts)
            << " fast_ok=" << stepTraceGet(StepCounter::UnorderedFastSuccess)
            << " fast_fail="
            << stepTraceGet(StepCounter::UnorderedFastFailures)
            << " strict=" << stepTraceGet(StepCounter::UnorderedStrictPasses)
            << " editable="
            << stepTraceGet(StepCounter::UnorderedEditablePasses) << '\n';
  std::cerr << "  repetition rule: calls="
            << stepTraceGet(StepCounter::RepetitionRecoverCalls)
            << " fast_try=" << stepTraceGet(StepCounter::RepetitionFastAttempts)
            << " fast_ok=" << stepTraceGet(StepCounter::RepetitionFastSuccess)
            << " fast_fail="
            << stepTraceGet(StepCounter::RepetitionFastFailures) << '\n';
}

#else

inline void stepTraceReset() noexcept {}
inline void stepTraceInc(StepCounter, std::uint64_t = 1) noexcept {}
template <typename... Args>
inline void stepTraceDumpSummary(Args &&...) noexcept {}

#endif

} // namespace pegium::parser::detail
