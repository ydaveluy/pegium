#pragma once

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <optional>
#include <string>
#include <string_view>


namespace pegium::documentation {

/// Produces rendered documentation snippets for AST declarations and grammar
/// elements.
class DocumentationProvider {
public:
  virtual ~DocumentationProvider() = default;

  /// Returns the rendered documentation of `node`, or `std::nullopt` when none is available.
  [[nodiscard]] virtual std::optional<std::string>
  getDocumentation(const AstNode &node) const = 0;

  /// Returns the documentation attached to a grammar element, or `std::nullopt`
  /// when none is available. The default implementation surfaces the doc of a
  /// documented keyword (`"class"_kw.doc("…")`); a hover position resolving to
  /// such a keyword renders this content.
  [[nodiscard]] virtual std::optional<std::string>
  getDocumentation(const grammar::AbstractElement &element) const {
    if (const auto *literal = dynamic_cast<const grammar::Literal *>(&element)) {
      const std::string_view documentation = literal->getDocumentation();
      if (!documentation.empty()) {
        return std::string{documentation};
      }
    }
    return std::nullopt;
  }
};

} // namespace pegium::documentation
