#pragma once

#include <algorithm>
#include <vector>

#include <pegium/parser/Parser.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::parser::detail {

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
  if (const auto hasSyntaxDiagnostic = [&diagnostics]() {
        for (const auto &diagnostic : diagnostics) {
          if (diagnostic.isSyntax()) {
            return true;
          }
        }
        return false;
      }();
      hasSyntaxDiagnostic) {
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
