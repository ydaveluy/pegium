#pragma once

/// Grammar contract for negative lookahead predicates.

#include <pegium/core/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct NotPredicate : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::NotPredicate;
  }

  virtual const AbstractElement *getElement() const noexcept = 0;
  constexpr ~NotPredicate() noexcept override = default;

  void print(std::ostream &os) const final;
};

} // namespace pegium::grammar
