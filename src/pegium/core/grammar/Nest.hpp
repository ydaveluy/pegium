#pragma once

/// Grammar contract for nested AST node creation.

#include <memory>
#include <string_view>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium::grammar {

struct Nest : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Nest;
  }
  constexpr ~Nest() noexcept override = default;

  virtual std::unique_ptr<AstNode>
  getValue(std::unique_ptr<AstNode> current) const = 0;
  virtual std::string_view getTypeName() const noexcept = 0;
  virtual std::string_view getFeature() const noexcept = 0;

  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
