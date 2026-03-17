#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <pegium/services/JsonValue.hpp>
#include <pegium/syntax-tree/CstNode.hpp>

namespace pegium::services {

enum class DiagnosticSeverity {
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4,
};

enum class DiagnosticTag {
  Unnecessary = 1,
  Deprecated = 2,
};

using DiagnosticCode = std::variant<std::int64_t, std::string>;

struct DiagnosticRelatedInformation {
  std::string uri;
  std::string message;
  TextOffset begin = 0;
  TextOffset end = 0;
};

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
