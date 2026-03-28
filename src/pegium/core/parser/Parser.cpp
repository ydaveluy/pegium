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

template <typename Diagnostic>
[[nodiscard]] bool can_merge_inserted(const Diagnostic &current,
                                      const Diagnostic &next) noexcept {
  return current.kind == ParseDiagnosticKind::Inserted &&
         next.kind == ParseDiagnosticKind::Inserted &&
         current.offset == next.offset && current.message == next.message;
}

template <typename Diagnostic>
[[nodiscard]] bool can_merge_deleted(const Diagnostic &current,
                                     const Diagnostic &next) noexcept {
  return current.kind == ParseDiagnosticKind::Deleted &&
         next.kind == ParseDiagnosticKind::Deleted &&
         next.beginOffset == current.endOffset &&
         current.message == next.message;
}

template <typename Diagnostic>
void merge_inserted(Diagnostic &current, const Diagnostic &next) {
  current.beginOffset = std::min(current.beginOffset, next.beginOffset);
  current.endOffset = std::max(current.endOffset, next.endOffset);
  if (current.element == nullptr) {
    current.element = next.element;
  }
}

template <typename Diagnostic>
void merge_deleted(Diagnostic &current, const Diagnostic &next) {
  current.endOffset = std::max(current.endOffset, next.endOffset);
}

template <typename Diagnostic>
[[nodiscard]] std::vector<Diagnostic>
normalize_diagnostics_impl(std::span<const Diagnostic> diagnostics) {
  std::vector<Diagnostic> normalized;
  normalized.reserve(diagnostics.size());

  for (const auto &diagnostic : diagnostics) {
    if (!normalized.empty()) {
      auto &current = normalized.back();
      if (can_merge_inserted(current, diagnostic)) {
        merge_inserted(current, diagnostic);
        continue;
      }
      if (can_merge_deleted(current, diagnostic)) {
        merge_deleted(current, diagnostic);
        continue;
      }
    }
    normalized.push_back(diagnostic);
  }

  return normalized;
}

} // namespace

std::ostream &operator<<(std::ostream &os, ParseDiagnosticKind kind) {
  return os << parse_diagnostic_kind_name(kind);
}

std::vector<detail::SyntaxScriptEntry>
detail::normalize_syntax_script(std::span<const SyntaxScriptEntry> entries) {
  return normalize_diagnostics_impl(entries);
}

std::vector<ParseDiagnostic>
detail::materialize_syntax_diagnostics(
    std::span<const SyntaxScriptEntry> entries) {
  std::vector<ParseDiagnostic> materialized;
  materialized.reserve(entries.size());
  for (const auto &entry : entries) {
    materialized.push_back({.kind = entry.kind,
                            .offset = entry.offset,
                            .beginOffset = entry.beginOffset,
                            .endOffset = entry.endOffset,
                            .element = entry.element,
                            .message = entry.message});
  }
  return materialized;
}

std::vector<ParseDiagnostic>
normalizeParseDiagnostics(std::span<const ParseDiagnostic> diagnostics) {
  return normalize_diagnostics_impl(diagnostics);
}

} // namespace pegium::parser
