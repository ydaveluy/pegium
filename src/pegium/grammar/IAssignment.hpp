#pragma once

#include <pegium/grammar/IGrammarElement.hpp>

namespace pegium::grammar {

struct IAssignment : IGrammarElement {
  virtual void execute(AstNode *current, const CstNode &node) const = 0;
};
} // namespace pegium::grammar