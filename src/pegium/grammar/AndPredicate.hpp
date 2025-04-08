#pragma once

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct AndPredicate : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::AndPredicate;
  }
  constexpr ~AndPredicate() noexcept override = default;
  virtual const AbstractElement *getElement() const noexcept = 0;
  void print(std::ostream &os) const final { os << '&' << *getElement(); }
};

} // namespace pegium::grammar