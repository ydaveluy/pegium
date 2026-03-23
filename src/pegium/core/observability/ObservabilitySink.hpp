#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <pegium/core/workspace/Symbol.hpp>

namespace pegium::workspace {
enum class DocumentState : std::uint8_t;
}

namespace pegium::observability {

/// Severity of one observability event.
enum class ObservationSeverity : std::uint8_t {
  Trace,
  Info,
  Warning,
  Error,
};

/// Stable code identifying one family of runtime observations.
enum class ObservationCode : std::uint8_t {
  WorkspaceDirectoryReadFailed,
  WorkspaceBootstrapFailed,
  DocumentUpdateDispatchFailed,
  LspRuntimeBackgroundTaskFailed,
  LanguageMappingCollision,
  ValidationCheckThrew,
  ValidationPreparationThrew,
  ValidationFinalizationThrew,
  ReferenceResolutionProblem,
};

/// Structured runtime observation emitted by core services.
struct Observation {
  ObservationSeverity severity = ObservationSeverity::Info;
  ObservationCode code = ObservationCode::WorkspaceDirectoryReadFailed;
  std::string message;
  std::string uri;
  std::string languageId;
  std::string category;
  workspace::DocumentId documentId = workspace::InvalidDocumentId;
  std::optional<workspace::DocumentState> state;
};

/// Sink receiving observability events from the runtime.
class ObservabilitySink {
public:
  virtual ~ObservabilitySink() noexcept = default;

  virtual void publish(const Observation &observation) noexcept = 0;
};

} // namespace pegium::observability
