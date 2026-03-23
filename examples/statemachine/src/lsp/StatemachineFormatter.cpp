#include "lsp/StatemachineFormatter.hpp"

namespace statemachine::services::lsp {
void StatemachineFormatter::formatStatemachine(
    pegium::FormattingBuilder &builder,
    const ast::Statemachine *model) const {
  auto formatter = builder.getNodeFormatter(model);
  formatter.keyword("statemachine").append(oneSpace);

  if (!model->events.empty()) {
    formatter.keyword("events").prepend(newLine);
    formatter.properties<&ast::Statemachine::events>().prepend(indent);
  }
  if (!model->commands.empty()) {
    formatter.keyword("commands").prepend(newLine);
    formatter.properties<&ast::Statemachine::commands>().prepend(indent);
  }

  formatter.keyword("initialState").prepend(newLine).append(oneSpace);
  formatter.properties<&ast::Statemachine::states>().prepend(newLines(2));
}

void StatemachineFormatter::formatState(pegium::FormattingBuilder &builder,
                                        const ast::State *state) const {
  auto formatter = builder.getNodeFormatter(state);
  formatter.keyword("state").append(oneSpace);

  if (!state->actions.empty()) {
    formatter.keyword("actions").prepend(oneSpace);
    auto openBrace = formatter.keyword("{");
    auto closeBrace = formatter.keyword("}");
    openBrace.prepend(oneSpace).append(oneSpace);
    closeBrace.prepend(oneSpace);
    formatter.properties<&ast::State::actions>().slice(1).prepend(oneSpace);
  }

  if (state->transitions.empty()) {
    formatter.keyword("end").prepend(oneSpace);
    return;
  }

  formatter.properties<&ast::State::transitions>().prepend(indent);
  formatter.keyword("end").prepend(newLine);
}

void StatemachineFormatter::formatTransition(
    pegium::FormattingBuilder &builder,
    const ast::Transition *transition) const {
  auto formatter = builder.getNodeFormatter(transition);
  formatter.keyword("=>").prepend(oneSpace).append(oneSpace);
}

StatemachineFormatter::StatemachineFormatter(
    const pegium::Services &services)
    : AbstractFormatter(services) {
  on<ast::Statemachine>(&StatemachineFormatter::formatStatemachine);
  on<ast::State>(&StatemachineFormatter::formatState);
  on<ast::Transition>(&StatemachineFormatter::formatTransition);
}

} // namespace statemachine::services::lsp
