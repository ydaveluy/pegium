#pragma once

#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/RuleValue.hpp>

namespace pegium {
class CstNodeView;
}

namespace pegium::grammar {

struct DataTypeRule : AbstractRule {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::DataTypeRule;
  }
  virtual RuleValue getValue(const CstNodeView &node) const = 0;
};

} // namespace pegium::grammar
