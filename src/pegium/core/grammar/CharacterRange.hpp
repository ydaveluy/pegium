#pragma once

/// Grammar contract for character-class terminals.

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

struct CharacterRange : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::CharacterRange;
  }
  constexpr ~CharacterRange() noexcept override = default;
  virtual std::string_view getValue(const CstNodeView &node) const noexcept;
};

} // namespace pegium::grammar
