#pragma once

/// Grammar contract for AST node creation elements.

#include <string_view>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium {
class AstArena;
} // namespace pegium

namespace pegium::grammar {

struct Create : AbstractElement {
  constexpr Create() noexcept : AbstractElement(ElementKind::Create) {}
  constexpr ~Create() noexcept override = default;

  virtual AstNode *getValue(AstArena &arena) const = 0;
  virtual std::string_view getTypeName() const noexcept = 0;

  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
