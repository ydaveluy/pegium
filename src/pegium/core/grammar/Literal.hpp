#pragma once

/// Grammar contract for literal and keyword terminals.

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>
#include <string_view>

namespace pegium::grammar {

struct Literal : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Literal;
  }

  constexpr ~Literal() noexcept override = default;
  virtual std::string_view getValue() const noexcept = 0;
  virtual std::string_view getValue(const CstNodeView &node) const noexcept;
  virtual bool isCaseSensitive() const noexcept = 0;
  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
