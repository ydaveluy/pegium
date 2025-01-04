#pragma once

#include <memory>
#include <pegium/grammar/IGrammarElement.hpp>

namespace pegium::grammar {

struct IAction : IGrammarElement {
  virtual std::shared_ptr<AstNode>
  execute(std::shared_ptr<AstNode> current) const = 0;
};
} // namespace pegium::grammar