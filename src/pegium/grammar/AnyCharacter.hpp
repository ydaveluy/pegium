#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct AnyCharacter : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::AnyCharacter;
  }
  constexpr ~AnyCharacter() noexcept override = default;
};

} // namespace pegium::grammar