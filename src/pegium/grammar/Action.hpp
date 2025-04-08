#pragma once

#include <memory>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree.hpp>
namespace pegium::grammar {

struct Action : AbstractElement {
  constexpr ~Action() noexcept override = default;

  virtual std::shared_ptr<AstNode>
  execute(std::shared_ptr<AstNode> current) const = 0;
};

} // namespace pegium::grammar