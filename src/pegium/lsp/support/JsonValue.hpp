#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/core/services/JsonValue.hpp>
#include <pegium/core/services/Diagnostic.hpp>

namespace pegium {

/// Converts one protocol value to the internal JSON representation.
[[nodiscard]] services::JsonValue from_lsp_any(const ::lsp::LSPAny &value);
/// Converts one internal JSON value to the protocol representation.
[[nodiscard]] ::lsp::LSPAny to_lsp_any(const services::JsonValue &value);

/// Converts an LSP diagnostic severity to the shared diagnostic model.
[[nodiscard]] services::DiagnosticSeverity
from_lsp_diagnostic_severity(::lsp::DiagnosticSeverity severity);
/// Converts a shared diagnostic severity to the LSP representation.
[[nodiscard]] ::lsp::DiagnosticSeverity
to_lsp_diagnostic_severity(services::DiagnosticSeverity severity);

} // namespace pegium
