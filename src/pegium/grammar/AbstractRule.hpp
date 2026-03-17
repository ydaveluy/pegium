#pragma once

#include <concepts>
#include <pegium/grammar/AbstractElement.hpp>
#include <string_view>
#include <type_traits>
#include <ostream>
namespace pegium::grammar {

struct AbstractRule : AbstractElement {
  constexpr ~AbstractRule() noexcept override = default;
  virtual std::string_view getTypeName() const noexcept = 0;
  virtual std::string_view getName() const noexcept = 0;

};

} // namespace pegium::grammar
