#pragma once

#include <pegium/syntax-tree/AstNode.hpp>
#include <optional>
#include <string>


namespace pegium::documentation {

class DocumentationProvider {
public:
  virtual ~DocumentationProvider() = default;
  [[nodiscard]] virtual std::optional<std::string>
  getDocumentation(const AstNode &node) const = 0;
};

} // namespace pegium::documentation
