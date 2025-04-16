#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct UnorderedGroup : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::UnorderedGroup;
  }
  // TODO add begin/end
  constexpr ~UnorderedGroup() noexcept override = default;
};

} // namespace pegium::grammar