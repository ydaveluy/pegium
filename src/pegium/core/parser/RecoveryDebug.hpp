#pragma once

/// JSON-oriented debug helpers for inspecting recovery internals.

#include <span>
#include <string_view>

#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/parser/RecoverySearch.hpp>
#include <pegium/core/services/JsonValue.hpp>

namespace pegium::parser::detail {

[[nodiscard]] std::string_view
recovery_attempt_status_name(RecoveryAttemptStatus status) noexcept;

[[nodiscard]] pegium::JsonValue
strict_parse_summary_to_json(const StrictParseSummary &summary);

[[nodiscard]] pegium::JsonValue
failure_snapshot_to_json(const FailureSnapshot &snapshot);

[[nodiscard]] pegium::JsonValue
recovery_window_to_json(const RecoveryWindow &window);

[[nodiscard]] pegium::JsonValue
recovery_windows_to_json(std::span<const RecoveryWindow> windows);

[[nodiscard]] pegium::JsonValue
recovery_attempt_to_json(const RecoveryAttempt &attempt,
                         const RecoveryAttemptSpec *spec = nullptr);

} // namespace pegium::parser::detail
