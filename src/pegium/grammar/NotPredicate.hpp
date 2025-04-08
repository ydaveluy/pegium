#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct NotPredicate : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::NotPredicate;
  }

  virtual const AbstractElement *getElement() const noexcept = 0;
  constexpr ~NotPredicate() noexcept override = default;
};

} // namespace pegium::grammar