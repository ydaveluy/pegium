#pragma once

/// Canonical syntax-script helpers and parser-facing diagnostic projection.

#include <algorithm>
#include <vector>

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::parser::detail {

struct SyntaxScriptEntry {
  ParseDiagnosticKind kind = ParseDiagnosticKind::Deleted;
  TextOffset offset = 0;
  TextOffset beginOffset = 0;
  TextOffset endOffset = 0;
  const grammar::AbstractElement *element = nullptr;
  std::string message;

  [[nodiscard]] constexpr bool isSyntax() const noexcept {
    return isSyntaxParseDiagnostic(kind);
  }
};

[[nodiscard]] std::vector<SyntaxScriptEntry>
normalize_syntax_script(std::span<const SyntaxScriptEntry> entries);

[[nodiscard]] std::vector<ParseDiagnostic>
materialize_syntax_diagnostics(std::span<const SyntaxScriptEntry> entries);

inline bool
has_syntax_diagnostic(std::span<const SyntaxScriptEntry> entries) {
  return std::ranges::any_of(entries, [](const auto &entry) {
    return entry.isSyntax();
  });
}

inline bool
has_parse_diagnostic_kind(std::span<const SyntaxScriptEntry> entries,
                          ParseDiagnosticKind kind) {
  return std::ranges::any_of(entries, [kind](const auto &entry) {
    return entry.kind == kind;
  });
}

inline CstNodeView first_recovered_node(const RootCstNode &cst) {
  for (NodeId id = 0;; ++id) {
    const auto node = cst.get(id);
    if (!node.valid()) {
      return {};
    }
    if (node.isRecovered()) {
      return node;
    }
  }
}

inline void
append_syntax_summary_entry(std::vector<SyntaxScriptEntry> &entries,
                            const RootCstNode *cst, TextOffset parsedLength,
                            TextOffset lastVisibleCursorOffset,
                            TextOffset failureVisibleCursorOffset,
                            TextOffset inputSize, bool hasRecovered,
                            bool fullMatch) {
  if (!fullMatch) {
    if (has_parse_diagnostic_kind(entries, ParseDiagnosticKind::Incomplete)) {
      return;
    }
    const auto incompleteOffset =
        std::max(parsedLength, failureVisibleCursorOffset);
    const auto safeInputSize = std::max(parsedLength, inputSize);
    const auto trailingUnexpectedInput =
        hasRecovered && parsedLength < safeInputSize;
    const auto recoveredTailBegin =
        std::min(parsedLength, lastVisibleCursorOffset);
    entries.push_back({.kind = ParseDiagnosticKind::Incomplete,
                       .offset = incompleteOffset,
                       .beginOffset =
                           trailingUnexpectedInput ? recoveredTailBegin
                                                   : incompleteOffset,
                       .endOffset =
                           trailingUnexpectedInput ? safeInputSize
                                                   : incompleteOffset,
                       .element = nullptr,
                       .message = trailingUnexpectedInput
                                      ? std::string("Unexpected input.")
                                      : std::string()});
    return;
  }
  if (has_syntax_diagnostic(entries)) {
    return;
  }
  if (!hasRecovered || cst == nullptr) {
    return;
  }
  if (const auto recovered = first_recovered_node(*cst); recovered.valid()) {
    entries.push_back({.kind = ParseDiagnosticKind::Recovered,
                       .offset = recovered.getBegin(),
                       .beginOffset = recovered.getBegin(),
                       .endOffset = recovered.getEnd(),
                       .element = nullptr,
                       .message = {}});
  }
}

} // namespace pegium::parser::detail
