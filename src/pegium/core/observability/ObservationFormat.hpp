#pragma once

#include <string>
#include <string_view>

#include <pegium/core/observability/ObservabilitySink.hpp>

namespace pegium::observability::detail {

/// Formats one observation as a human-readable log line.
[[nodiscard]] std::string format_observation(const Observation &observation);

[[nodiscard]] std::string_view
to_string(ObservationSeverity severity) noexcept;
[[nodiscard]] std::string_view to_string(ObservationCode code) noexcept;
[[nodiscard]] std::string_view
to_string(workspace::DocumentState state) noexcept;

} // namespace pegium::observability::detail
