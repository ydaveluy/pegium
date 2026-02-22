#pragma once

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

struct CharacterRange : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::CharacterRange;
  }
  constexpr ~CharacterRange() noexcept override = default;
  virtual std::string_view getValue(const CstNodeView &node) const noexcept {
    return node.getText();
  }
};

} // namespace pegium::grammar
