#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct AndPredicate : AbstractElement {
    constexpr ElementKind getKind() const noexcept final {
        return ElementKind::AndPredicate;
      }
      constexpr ~AndPredicate() noexcept override = default;
      virtual const AbstractElement* getElement()const noexcept = 0;
};

} // namespace pegium::grammar