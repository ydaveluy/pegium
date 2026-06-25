#pragma once

/// Grammar contract for nested AST node creation.

#include <string_view>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium {
class AstArena;
} // namespace pegium

namespace pegium::grammar {

struct Nest : AbstractElement {
  constexpr Nest() noexcept : AbstractElement(ElementKind::Nest) {}
  constexpr ~Nest() noexcept override = default;

  virtual AstNode *getValue(AstNode *current, AstArena &arena) const = 0;
  virtual std::string_view getTypeName() const noexcept = 0;
  virtual std::string_view getFeature() const noexcept = 0;

  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
