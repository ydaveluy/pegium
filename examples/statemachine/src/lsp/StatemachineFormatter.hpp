#pragma once

#include <statemachine/ast.hpp>

#include <pegium/lsp/formatting/AbstractFormatter.hpp>

namespace statemachine::lsp {

class StatemachineFormatter : public pegium::AbstractFormatter {
public:
  explicit StatemachineFormatter(const pegium::Services &services);

protected:
  virtual void formatStatemachine(pegium::FormattingBuilder &builder,
                                  const ast::Statemachine *model) const;
  virtual void formatState(pegium::FormattingBuilder &builder,
                           const ast::State *state) const;
  virtual void formatTransition(pegium::FormattingBuilder &builder,
                                const ast::Transition *transition) const;
};

} // namespace statemachine::lsp
