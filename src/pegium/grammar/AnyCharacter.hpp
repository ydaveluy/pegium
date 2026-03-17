#pragma once

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

struct AnyCharacter : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::AnyCharacter;
  }
  constexpr ~AnyCharacter() noexcept override = default;
  virtual std::string_view getValue(const CstNodeView &node) const noexcept;

  void print(std::ostream &os) const final;
};

} // namespace pegium::grammar
