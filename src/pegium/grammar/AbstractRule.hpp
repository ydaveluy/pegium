#pragma once

#include <pegium/grammar/AbstractElement.hpp>
#include <string_view>

namespace pegium::grammar {

struct AbstractRule : AbstractElement {
  constexpr ~AbstractRule() noexcept override = default;
  virtual const AbstractElement *getElement() const noexcept = 0;
  virtual std::string_view getTypeName() const noexcept = 0;
};

} // namespace pegium::grammar
