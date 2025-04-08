#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct Group : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Group;
  }
  // TODO add begin/end
  constexpr ~Group() noexcept override = default;
};

} // namespace pegium::grammar