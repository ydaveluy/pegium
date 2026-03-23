#pragma once

/// Grammar contract for ordered sequences of child elements.

#include <cstddef>
#include <pegium/core/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct Group : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Group;
  }
  /// get the element at the given index in this group.
  virtual const AbstractElement *get(std::size_t index) const noexcept = 0;
  /// get the number of elements in this group. 
  virtual std::size_t size() const noexcept = 0;
  constexpr ~Group() noexcept override = default;
  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
