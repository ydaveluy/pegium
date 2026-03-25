#include <pegium/lsp/support/JsonValue.hpp>

#include <cmath>
#include <limits>

namespace pegium {

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

pegium::JsonValue from_lsp_any(const ::lsp::LSPAny &value) {
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
    pegium::JsonValue::Array out;
    out.reserve(value.array().size());
    for (const auto &entry : value.array()) {
      out.push_back(from_lsp_any(entry));
    }
    return out;
  }
  if (value.isObject()) {
    pegium::JsonValue::Object out;
    for (const auto &[key, entry] : value.object().keyValueMap()) {
      out.try_emplace(key, from_lsp_any(entry));
    }
    return out;
  }
  return nullptr;
}

::lsp::LSPAny to_lsp_any(const pegium::JsonValue &value) {
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

pegium::DiagnosticSeverity
from_lsp_diagnostic_severity(::lsp::DiagnosticSeverity severity) {
  using enum ::lsp::DiagnosticSeverity;
  switch (severity) {
  case Error:
    return pegium::DiagnosticSeverity::Error;
  case Warning:
    return pegium::DiagnosticSeverity::Warning;
  case Information:
    return pegium::DiagnosticSeverity::Information;
  case Hint:
    return pegium::DiagnosticSeverity::Hint;
  case MAX_VALUE:
    break;
  }
  return pegium::DiagnosticSeverity::Error;
}

::lsp::DiagnosticSeverity
to_lsp_diagnostic_severity(pegium::DiagnosticSeverity severity) {
  using enum pegium::DiagnosticSeverity;
  switch (severity) {
  case Error:
    return ::lsp::DiagnosticSeverity::Error;
  case Warning:
    return ::lsp::DiagnosticSeverity::Warning;
  case Information:
    return ::lsp::DiagnosticSeverity::Information;
  case Hint:
    return ::lsp::DiagnosticSeverity::Hint;
  }
  return ::lsp::DiagnosticSeverity::Error;
}

} // namespace pegium
