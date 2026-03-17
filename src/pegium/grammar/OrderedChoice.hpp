#pragma once

#include <cstddef>
#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct OrderedChoice : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::OrderedChoice;
  }
  /// get the element at the given index in this ordered choice.
  virtual const AbstractElement *get(std::size_t index) const noexcept = 0;
  /// get the number of elements in this ordered choice. 
  virtual std::size_t size() const noexcept = 0;
  constexpr ~OrderedChoice() noexcept override = default;
  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
