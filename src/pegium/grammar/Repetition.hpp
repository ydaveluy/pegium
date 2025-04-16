#pragma once
#include <cstdint>
#include <pegium/grammar/AbstractElement.hpp>

namespace pegium::grammar {

struct Repetition : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Repetition;
  }

  constexpr ~Repetition() noexcept override = default;
  virtual std::size_t getMin() const noexcept = 0;
  virtual std::size_t getMax() const noexcept = 0;

  virtual const AbstractElement *getElement() const noexcept = 0;
};

} // namespace pegium::grammar