#include <pegium/core/parser/PegiumParser.hpp>

#include <algorithm>
#include <ostream>
#include <string_view>
#include <vector>

namespace pegium::parser {
namespace {

[[nodiscard]] constexpr std::string_view
parse_diagnostic_kind_name(ParseDiagnosticKind kind) noexcept {
  using enum ParseDiagnosticKind;
  switch (kind) {
  case Inserted:
    return "Inserted";
  case Deleted:
    return "Deleted";
  case Replaced:
    return "Replaced";
  case Incomplete:
    return "Incomplete";
  case Recovered:
    return "Recovered";
  case ConversionError:
    return "ConversionError";
  }
  return "Unknown";
}

[[nodiscard]] constexpr bool
can_merge_inserted(const ParseDiagnostic &current,
                   const ParseDiagnostic &next) noexcept {
  return current.kind == ParseDiagnosticKind::Inserted &&
         next.kind == ParseDiagnosticKind::Inserted &&
         current.offset == next.offset && current.message.empty() &&
         next.message.empty();
}

[[nodiscard]] constexpr bool
can_merge_deleted(const ParseDiagnostic &current,
                  const ParseDiagnostic &next) noexcept {
  return current.kind == ParseDiagnosticKind::Deleted &&
         next.kind == ParseDiagnosticKind::Deleted &&
         next.offset == current.endOffset && current.message.empty() &&
         next.message.empty();
}

} // namespace

std::ostream &operator<<(std::ostream &os, ParseDiagnosticKind kind) {
  return os << parse_diagnostic_kind_name(kind);
}

std::vector<ParseDiagnostic>
normalizeParseDiagnostics(std::span<const ParseDiagnostic> diagnostics) {
  std::vector<ParseDiagnostic> normalized;
  normalized.reserve(diagnostics.size());

  for (const auto &diagnostic : diagnostics) {
    if (!normalized.empty()) {
      auto &current = normalized.back();
      if (can_merge_inserted(current, diagnostic)) {
        current.beginOffset = std::min(current.beginOffset, diagnostic.beginOffset);
        current.endOffset = std::max(current.endOffset, diagnostic.endOffset);
        if (current.element != diagnostic.element) {
          current.element = nullptr;
        }
        continue;
      }
      if (can_merge_deleted(current, diagnostic)) {
        current.endOffset = std::max(current.endOffset, diagnostic.endOffset);
        continue;
      }
    }
    normalized.push_back(diagnostic);
  }

  return normalized;
}

} // namespace pegium::parser
