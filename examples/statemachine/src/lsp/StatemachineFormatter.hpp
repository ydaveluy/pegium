#pragma once

#include <statemachine/ast.hpp>

#include <pegium/lsp/AbstractFormatter.hpp>

namespace statemachine::services::lsp {

class StatemachineFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit StatemachineFormatter(const pegium::services::Services &services);

protected:
  virtual void formatStatemachine(pegium::lsp::FormattingBuilder &builder,
                                  const ast::Statemachine *model) const;
  virtual void formatState(pegium::lsp::FormattingBuilder &builder,
                           const ast::State *state) const;
  virtual void formatTransition(pegium::lsp::FormattingBuilder &builder,
                                const ast::Transition *transition) const;
};

} // namespace statemachine::services::lsp
