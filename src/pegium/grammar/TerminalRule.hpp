#pragma once

#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/RuleValue.hpp>

namespace pegium {
class CstNodeView;
}

namespace pegium::grammar {

struct TerminalRule : AbstractRule {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::TerminalRule;
  }
  virtual RuleValue getValue(const CstNodeView &node) const = 0;
};

} // namespace pegium::grammar
