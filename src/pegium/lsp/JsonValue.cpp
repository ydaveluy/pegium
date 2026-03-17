#include <pegium/lsp/JsonValue.hpp>

#include <cmath>
#include <limits>

namespace pegium::lsp {

namespace {

constexpr ::lsp::json::Integer
to_lsp_integer(std::int64_t value) noexcept {
  if (value < std::numeric_limits<::lsp::json::Integer>::min()) {
    return std::numeric_limits<::lsp::json::Integer>::min();
  }
  if (value > std::numeric_limits<::lsp::json::Integer>::max()) {
    return std::numeric_limits<::lsp::json::Integer>::max();
  }
  return static_cast<::lsp::json::Integer>(value);
}

} // namespace

services::JsonValue from_lsp_any(const ::lsp::LSPAny &value) {
  if (value.isNull()) {
    return nullptr;
  }
  if (value.isBoolean()) {
    return value.boolean();
  }
  if (value.isString()) {
    return std::string(value.string());
  }
  if (value.isInteger()) {
    return static_cast<std::int64_t>(value.integer());
  }
  if (value.isNumber()) {
    return value.number();
  }
  if (value.isArray()) {
    services::JsonValue::Array out;
    out.reserve(value.array().size());
    for (const auto &entry : value.array()) {
      out.push_back(from_lsp_any(entry));
    }
    return out;
  }
  if (value.isObject()) {
    services::JsonValue::Object out;
    for (const auto &[key, entry] : value.object().keyValueMap()) {
      out.emplace(key, from_lsp_any(entry));
    }
    return out;
  }
  return nullptr;
}

::lsp::LSPAny to_lsp_any(const services::JsonValue &value) {
  if (value.isNull()) {
    return nullptr;
  }
  if (value.isBoolean()) {
    return value.boolean();
  }
  if (value.isString()) {
    return ::lsp::String(value.string());
  }
  if (value.isInteger()) {
    return to_lsp_integer(value.integer());
  }
  if (value.isNumber()) {
    return value.number();
  }
  if (value.isArray()) {
    ::lsp::LSPArray out;
    out.reserve(value.array().size());
    for (const auto &entry : value.array()) {
      out.push_back(to_lsp_any(entry));
    }
    return out;
  }
  if (value.isObject()) {
    ::lsp::LSPObject out;
    for (const auto &[key, entry] : value.object()) {
      out[key] = to_lsp_any(entry);
    }
    return out;
  }
  return nullptr;
}

services::DiagnosticSeverity
from_lsp_diagnostic_severity(::lsp::DiagnosticSeverity severity) {
  switch (severity) {
  case ::lsp::DiagnosticSeverity::Error:
    return services::DiagnosticSeverity::Error;
  case ::lsp::DiagnosticSeverity::Warning:
    return services::DiagnosticSeverity::Warning;
  case ::lsp::DiagnosticSeverity::Information:
    return services::DiagnosticSeverity::Information;
  case ::lsp::DiagnosticSeverity::Hint:
    return services::DiagnosticSeverity::Hint;
  case ::lsp::DiagnosticSeverity::MAX_VALUE:
    break;
  }
  return services::DiagnosticSeverity::Error;
}

::lsp::DiagnosticSeverity
to_lsp_diagnostic_severity(services::DiagnosticSeverity severity) {
  switch (severity) {
  case services::DiagnosticSeverity::Error:
    return ::lsp::DiagnosticSeverity::Error;
  case services::DiagnosticSeverity::Warning:
    return ::lsp::DiagnosticSeverity::Warning;
  case services::DiagnosticSeverity::Information:
    return ::lsp::DiagnosticSeverity::Information;
  case services::DiagnosticSeverity::Hint:
    return ::lsp::DiagnosticSeverity::Hint;
  }
  return ::lsp::DiagnosticSeverity::Error;
}

} // namespace pegium::lsp
