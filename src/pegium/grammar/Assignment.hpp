#pragma once
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>

namespace pegium::grammar {

struct Assignment : AbstractElement {
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Assignment;
  }
  constexpr ~Assignment() noexcept override = default;
  virtual void execute(AstNode *current, const CstNode &node) const = 0;
  virtual const AbstractElement *getElement() const noexcept = 0;
  virtual std::string_view getFeature() const noexcept = 0;
};

} // namespace pegium::grammar