#pragma once

#include <span>
#include <string_view>

#include <pegium/parser/RecoveryAnalysis.hpp>
#include <pegium/parser/RecoverySearch.hpp>
#include <pegium/services/JsonValue.hpp>

namespace pegium::parser::detail {

[[nodiscard]] std::string_view
recovery_attempt_status_name(RecoveryAttemptStatus status) noexcept;

[[nodiscard]] services::JsonValue
strict_parse_summary_to_json(const StrictParseSummary &summary);

[[nodiscard]] services::JsonValue
failure_snapshot_to_json(const FailureSnapshot &snapshot);

[[nodiscard]] services::JsonValue
recovery_window_to_json(const RecoveryWindow &window);

[[nodiscard]] services::JsonValue
recovery_windows_to_json(std::span<const RecoveryWindow> windows);

[[nodiscard]] services::JsonValue
recovery_attempt_to_json(const RecoveryAttempt &attempt,
                         const RecoveryAttemptSpec *spec = nullptr);

} // namespace pegium::parser::detail
