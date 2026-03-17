#include "validation/StatemachineValidator.hpp"

#include <statemachine/ast.hpp>

#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <pegium/validation/DiagnosticRanges.hpp>

namespace statemachine::services::validation {
namespace {

using namespace statemachine::ast;

} // namespace

void StatemachineValidator::registerValidationChecks(
    pegium::validation::ValidationRegistry &registry,
    const pegium::services::Services & /*services*/) {
  const StatemachineValidator validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkStateNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkUniqueNames>(validator)});
}

void StatemachineValidator::checkStateNameStartsWithCapital(
    const State &state,
    const pegium::validation::ValidationAcceptor &accept) const {
  if (state.name.empty()) {
    return;
  }
  const auto first = state.name.front();
  if (std::toupper(static_cast<unsigned char>(first)) != first) {
    accept.warning(state, "State name should start with a capital letter.")
        .property<&State::name>();
  }
}

void StatemachineValidator::checkUniqueNames(
    const Statemachine &model,
    const pegium::validation::ValidationAcceptor &accept) const {
  std::unordered_map<std::string, std::vector<const pegium::AstNode *>> names;
  for (const auto &event : model.events) {
    if (event) {
      names[event->name].push_back(event.get());
    }
  }
  for (const auto &state : model.states) {
    if (state) {
      names[state->name].push_back(state.get());
    }
  }

  const auto acceptDuplicate = [&](const pegium::AstNode &node,
                                   std::string_view name) {
    if (const auto *event = dynamic_cast<const Event *>(&node)) {
      accept.error(*event, "Duplicate identifier name: " + std::string(name))
          .property<&Event::name>();
      return;
    }
    if (const auto *state = dynamic_cast<const State *>(&node)) {
      accept.error(*state, "Duplicate identifier name: " + std::string(name))
          .property<&State::name>();
      return;
    }
    accept.error(node, "Duplicate identifier name: " + std::string(name));
  };

  for (const auto &[name, nodes] : names) {
    if (nodes.size() <= 1) {
      continue;
    }
    for (const auto *node : nodes) {
      acceptDuplicate(*node, name);
    }
  }
}

} // namespace statemachine::services::validation
