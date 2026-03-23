#include <pegium/core/observability/ObservationFormat.hpp>

#include <algorithm>
#include <format>
#include <string>

#include <pegium/core/workspace/Document.hpp>

namespace pegium::observability::detail {
namespace {

std::string sanitize_single_line(std::string_view text) {
  std::string sanitized(text);
  std::ranges::replace(sanitized, '\n', ' ');
  std::ranges::replace(sanitized, '\r', ' ');
  return sanitized;
}

void append_field(std::string &out, std::string_view key, std::string_view value) {
  if (value.empty()) {
    return;
  }
  out += " ";
  out += key;
  out += "=";
  out += sanitize_single_line(value);
}

} // namespace

std::string format_observation(const Observation &observation) {
  std::string formatted;
  formatted.reserve(observation.message.size() + 96);
  formatted += std::string(to_string(observation.severity));
  formatted += " ";
  formatted += std::string(to_string(observation.code));
  formatted += ": ";
  formatted += sanitize_single_line(observation.message);
  append_field(formatted, "uri", observation.uri);
  append_field(formatted, "languageId", observation.languageId);
  append_field(formatted, "category", observation.category);
  if (observation.documentId != workspace::InvalidDocumentId) {
    formatted += std::format(" documentId={}", observation.documentId);
  }
  if (observation.state.has_value()) {
    formatted += " state=" + std::string(to_string(*observation.state));
  }
  return formatted;
}

std::string_view to_string(ObservationSeverity severity) noexcept {
  using enum ObservationSeverity;
  switch (severity) {
  case Trace:
    return "trace";
  case Info:
    return "info";
  case Warning:
    return "warning";
  case Error:
    return "error";
  }
  return "info";
}

std::string_view to_string(ObservationCode code) noexcept {
  using enum ObservationCode;
  switch (code) {
  case WorkspaceDirectoryReadFailed:
    return "WorkspaceDirectoryReadFailed";
  case WorkspaceBootstrapFailed:
    return "WorkspaceBootstrapFailed";
  case DocumentUpdateDispatchFailed:
    return "DocumentUpdateDispatchFailed";
  case LspRuntimeBackgroundTaskFailed:
    return "LspRuntimeBackgroundTaskFailed";
  case LanguageMappingCollision:
    return "LanguageMappingCollision";
  case ValidationCheckThrew:
    return "ValidationCheckThrew";
  case ValidationPreparationThrew:
    return "ValidationPreparationThrew";
  case ValidationFinalizationThrew:
    return "ValidationFinalizationThrew";
  case ReferenceResolutionProblem:
    return "ReferenceResolutionProblem";
  }
  return "Observation";
}

std::string_view to_string(workspace::DocumentState state) noexcept {
  using enum workspace::DocumentState;
  switch (state) {
  case Changed:
    return "Changed";
  case Parsed:
    return "Parsed";
  case IndexedContent:
    return "IndexedContent";
  case ComputedScopes:
    return "ComputedScopes";
  case Linked:
    return "Linked";
  case IndexedReferences:
    return "IndexedReferences";
  case Validated:
    return "Validated";
  }
  return "Changed";
}

} // namespace pegium::observability::detail
