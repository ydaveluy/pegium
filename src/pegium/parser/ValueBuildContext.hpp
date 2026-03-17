#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <pegium/parser/Parser.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/syntax-tree/Reference.hpp>

namespace pegium::parser {

struct ValueBuildContext {
  std::vector<ReferenceHandle> *references = nullptr;
  const references::Linker *linker = nullptr;
  std::string_view property = {};
  std::vector<ParseDiagnostic> *diagnostics = nullptr;

  [[nodiscard]] ValueBuildContext
  withProperty(std::string_view nextProperty) const noexcept {
    auto copy = *this;
    copy.property = nextProperty;
    return copy;
  }

  void addConversionDiagnostic(const CstNodeView &node,
                               const grammar::AbstractElement *element,
                               std::string message) const {
    if (diagnostics == nullptr) {
      return;
    }
    diagnostics->push_back(ParseDiagnostic{
        .kind = ParseDiagnosticKind::ConversionError,
        .offset = node.getBegin(),
        .beginOffset = node.getBegin(),
        .endOffset = node.getEnd(),
        .element = element,
        .message = std::move(message),
    });
  }
};

} // namespace pegium::parser
