#pragma once

/// Helpers that synthesize parse diagnostics from CST and parse summaries.

#include <algorithm>
#include <vector>

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::parser::detail {

inline bool has_syntax_diagnostic(
    const std::vector<ParseDiagnostic> &diagnostics) {
  return std::ranges::any_of(diagnostics, [](const auto &diagnostic) {
    return diagnostic.isSyntax();
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

inline void ensure_parse_diagnostic(std::vector<ParseDiagnostic> &diagnostics,
                                   const RootCstNode *cst,
                                   TextOffset parsedLength,
                                   TextOffset failureVisibleCursorOffset,
                                   bool fullMatch) {
  if (has_syntax_diagnostic(diagnostics)) {
    return;
  }
  if (!fullMatch) {
    const auto incompleteOffset =
        std::max(parsedLength, failureVisibleCursorOffset);
    diagnostics.push_back({.kind = ParseDiagnosticKind::Incomplete,
                           .offset = incompleteOffset,
                           .beginOffset = incompleteOffset,
                           .endOffset = incompleteOffset,
                           .element = nullptr,
                           .message = {}});
    return;
  }
  if (cst == nullptr) {
    return;
  }
  if (const auto recovered = first_recovered_node(*cst); recovered.valid()) {
    diagnostics.push_back({.kind = ParseDiagnosticKind::Recovered,
                           .offset = recovered.getBegin(),
                           .beginOffset = recovered.getBegin(),
                           .endOffset = recovered.getEnd(),
                           .element = nullptr,
                           .message = {}});
  }
}

} // namespace pegium::parser::detail
