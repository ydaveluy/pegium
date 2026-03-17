#pragma once

#include <memory>
#include <string_view>

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree/AstNode.hpp>

namespace pegium::grammar {

struct Create : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Create;
  }
  constexpr ~Create() noexcept override = default;

  virtual std::unique_ptr<AstNode> getValue() const = 0;
  virtual std::string_view getTypeName() const noexcept = 0;

  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
