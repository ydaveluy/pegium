#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/services/JsonValue.hpp>
#include <pegium/services/Diagnostic.hpp>

namespace pegium::lsp {

[[nodiscard]] services::JsonValue from_lsp_any(const ::lsp::LSPAny &value);
[[nodiscard]] ::lsp::LSPAny to_lsp_any(const services::JsonValue &value);

[[nodiscard]] services::DiagnosticSeverity
from_lsp_diagnostic_severity(::lsp::DiagnosticSeverity severity);
[[nodiscard]] ::lsp::DiagnosticSeverity
to_lsp_diagnostic_severity(services::DiagnosticSeverity severity);

} // namespace pegium::lsp
