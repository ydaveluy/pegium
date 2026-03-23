#pragma once

#include <string>
#include <vector>

#include <pegium/core/services/Diagnostic.hpp>

namespace pegium::workspace {

/// Text-plus-diagnostics snapshot commonly used by editor-facing updates.
struct DocumentDiagnosticsSnapshot {
  std::string uri;
  std::string text;
  std::vector<services::Diagnostic> diagnostics;
};

} // namespace pegium::workspace
