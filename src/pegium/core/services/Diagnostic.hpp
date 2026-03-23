#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <pegium/core/services/JsonValue.hpp>
#include <pegium/core/syntax-tree/CstNode.hpp>

namespace pegium::services {

/// Severity level of one diagnostic entry.
enum class DiagnosticSeverity {
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4,
};

/// Extra UI hints attached to one diagnostic.
enum class DiagnosticTag {
  Unnecessary = 1,
  Deprecated = 2,
};

/// Stable machine-readable diagnostic identifier.
using DiagnosticCode = std::variant<std::int64_t, std::string>;

/// Additional location associated with a primary diagnostic.
struct DiagnosticRelatedInformation {
  std::string uri;
  std::string message;
  TextOffset begin = 0;
  TextOffset end = 0;
};

/// Generic diagnostic payload emitted by parsing, linking, or validation.
struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  std::string message;
  std::string source;
  std::optional<DiagnosticCode> code;
  std::optional<std::string> codeDescription;
  std::vector<DiagnosticTag> tags;
  std::vector<DiagnosticRelatedInformation> relatedInformation;
  std::optional<JsonValue> data;
  TextOffset begin = 0;
  TextOffset end = 0;
};

} // namespace pegium::services
