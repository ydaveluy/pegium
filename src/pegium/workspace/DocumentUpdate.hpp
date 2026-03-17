#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <pegium/services/Diagnostic.hpp>

namespace pegium::workspace {

struct DocumentDiagnosticsSnapshot {
  std::string uri;
  std::optional<std::int64_t> version;
  std::string text;
  std::vector<services::Diagnostic> diagnostics;
};

} // namespace pegium::workspace
