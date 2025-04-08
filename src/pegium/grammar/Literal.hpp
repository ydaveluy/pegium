#pragma once

#include <pegium/grammar/AbstractElement.hpp>
#include <string_view>

namespace pegium::grammar {

struct Literal : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Literal;
  }

  constexpr ~Literal() noexcept override = default;
  virtual std::string_view getValue() const noexcept = 0;
  virtual bool isCaseSensitive() const noexcept = 0;
};

} // namespace pegium::grammar