#include <pegium/lsp/support/JsonValue.hpp>

#include <limits>

namespace pegium {

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
    // LSP `integer` is 32-bit on the wire; a 64-bit value outside that range
    // round-trips through `decimal` (a double, exact up to 2^53) — mirroring how
    // the JSON parser reads such literals back — instead of saturating to a
    // wrong-but-bounded value.
    const auto integer = value.integer();
    if (integer < std::numeric_limits<::lsp::json::Integer>::min() ||
        integer > std::numeric_limits<::lsp::json::Integer>::max()) {
      return static_cast<::lsp::json::Decimal>(integer);
    }
    return static_cast<::lsp::json::Integer>(integer);
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
