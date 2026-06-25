#pragma once

/// Grammar contract for literal and keyword terminals.

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>
#include <string_view>

namespace pegium::grammar {

struct Literal : AbstractElement {
  constexpr Literal() noexcept : AbstractElement(ElementKind::Literal) {}

  constexpr ~Literal() noexcept override = default;
  virtual std::string_view getValue() const noexcept = 0;
  virtual std::string_view getValue(const CstNodeView &node) const noexcept;
  virtual bool isCaseSensitive() const noexcept = 0;
  /// Optional human-readable documentation attached to this keyword (e.g. via
  /// `"class"_kw.doc("…")`). Empty when none was provided; surfaced on hover
  /// through the DocumentationProvider.
  virtual std::string_view getDocumentation() const noexcept { return {}; }
  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
