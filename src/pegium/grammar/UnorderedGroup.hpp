#pragma once

#include <cstddef>
#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct UnorderedGroup : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::UnorderedGroup;
  }
  /// get the element at the given index in this unordered group.
  virtual const AbstractElement *get(std::size_t index) const noexcept = 0;
  /// get the number of elements in this unordered group. 
  virtual std::size_t size() const noexcept = 0;
  constexpr ~UnorderedGroup() noexcept override = default;
  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
