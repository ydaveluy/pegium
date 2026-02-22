#pragma once

#include <memory>
#include <pegium/grammar/AbstractRule.hpp>

namespace pegium {
class AstNode;
class CstNodeView;
}

namespace pegium::grammar {

struct ParserRule : AbstractRule {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }
  virtual std::shared_ptr<AstNode>
  getValue(const CstNodeView &) const = 0;
};

} // namespace pegium::grammar
