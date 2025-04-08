#pragma once
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>

namespace pegium::grammar {

struct CrossReference : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::CrossReference;
  }

  constexpr ~CrossReference() noexcept override = default;
  virtual const AbstractElement *getElement() const noexcept = 0;
  virtual std::type_info getType() const noexcept = 0;
  void print(std::ostream &os) const final {
    os << '[' << getType().name() << ':' << *getElement() << ']';
  }
};

} // namespace pegium::grammar