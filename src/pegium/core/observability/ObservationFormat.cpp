#include <pegium/core/observability/ObservationFormat.hpp>

#include <algorithm>
#include <string>

#include <pegium/core/workspace/Document.hpp>

namespace pegium::observability::detail {
namespace {

std::string sanitize_single_line(std::string_view text) {
  std::string sanitized(text);
  std::replace(sanitized.begin(), sanitized.end(), '\n', ' ');
  std::replace(sanitized.begin(), sanitized.end(), '\r', ' ');
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
    formatted += " documentId=" + std::to_string(observation.documentId);
  }
  if (observation.state.has_value()) {
    formatted += " state=" + std::string(to_string(*observation.state));
  }
  return formatted;
}

std::string_view to_string(ObservationSeverity severity) noexcept {
  switch (severity) {
  case ObservationSeverity::Trace:
    return "trace";
  case ObservationSeverity::Info:
    return "info";
  case ObservationSeverity::Warning:
    return "warning";
  case ObservationSeverity::Error:
    return "error";
  }
  return "info";
}

std::string_view to_string(ObservationCode code) noexcept {
  switch (code) {
  case ObservationCode::WorkspaceDirectoryReadFailed:
    return "WorkspaceDirectoryReadFailed";
  case ObservationCode::WorkspaceBootstrapFailed:
    return "WorkspaceBootstrapFailed";
  case ObservationCode::DocumentUpdateDispatchFailed:
    return "DocumentUpdateDispatchFailed";
  case ObservationCode::LspRuntimeBackgroundTaskFailed:
    return "LspRuntimeBackgroundTaskFailed";
  case ObservationCode::LanguageMappingCollision:
    return "LanguageMappingCollision";
  case ObservationCode::ValidationCheckThrew:
    return "ValidationCheckThrew";
  case ObservationCode::ValidationPreparationThrew:
    return "ValidationPreparationThrew";
  case ObservationCode::ValidationFinalizationThrew:
    return "ValidationFinalizationThrew";
  case ObservationCode::ReferenceResolutionProblem:
    return "ReferenceResolutionProblem";
  }
  return "Observation";
}

std::string_view to_string(workspace::DocumentState state) noexcept {
  switch (state) {
  case workspace::DocumentState::Changed:
    return "Changed";
  case workspace::DocumentState::Parsed:
    return "Parsed";
  case workspace::DocumentState::IndexedContent:
    return "IndexedContent";
  case workspace::DocumentState::ComputedScopes:
    return "ComputedScopes";
  case workspace::DocumentState::Linked:
    return "Linked";
  case workspace::DocumentState::IndexedReferences:
    return "IndexedReferences";
  case workspace::DocumentState::Validated:
    return "Validated";
  }
  return "Changed";
}

} // namespace pegium::observability::detail
