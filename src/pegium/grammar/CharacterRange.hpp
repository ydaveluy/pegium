#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct CharacterRange : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::CharacterRange;
  }
  constexpr ~CharacterRange() noexcept override = default;
};

} // namespace pegium::grammar