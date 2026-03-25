#pragma once

#include <ostream>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/AbstractRule.hpp>

namespace pegium::grammar::detail {

[[nodiscard]] constexpr bool is_rule_element_kind(ElementKind kind) noexcept {
  using enum ElementKind;
  switch (kind) {
  case DataTypeRule:
  case ParserRule:
  case TerminalRule:
  case InfixRule:
    return true;
  default:
    return false;
  }
}

inline std::ostream &print_element_reference(std::ostream &os,
                                             const AbstractElement &element) {
  if (is_rule_element_kind(element.getKind())) {
    return os << static_cast<const AbstractRule &>(element).getName();
  }
  return os << element;
}

} // namespace pegium::grammar::detail
