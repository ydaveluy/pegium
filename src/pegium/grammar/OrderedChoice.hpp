#pragma once

#include <pegium/grammar/AbstractElement.hpp>


namespace pegium::grammar {

struct OrderedChoice : AbstractElement {
    constexpr ElementKind getKind() const noexcept final {
        return ElementKind::OrderedChoice;
      }
      // TODO add begin/end
      constexpr ~OrderedChoice() noexcept override = default;
};

} // namespace pegium::grammar