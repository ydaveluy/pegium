#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <optional>
#include <string>


namespace pegium::documentation {

/// Produces rendered documentation snippets for AST declarations.
class DocumentationProvider {
public:
  virtual ~DocumentationProvider() = default;

  /// Returns the rendered documentation of `node`, or `std::nullopt` when none is available.
  [[nodiscard]] virtual std::optional<std::string>
  getDocumentation(const AstNode &node) const = 0;
};

} // namespace pegium::documentation
