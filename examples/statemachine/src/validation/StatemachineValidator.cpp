#include "validation/StatemachineValidator.hpp"

#include <statemachine/ast.hpp>

#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace statemachine::services::validation {
namespace {

using namespace statemachine::ast;

} // namespace

void registerValidationChecks(
    statemachine::services::StatemachineServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.statemachine.validation.statemachineValidator;

  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkStateNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkUniqueStatesAndEvents>(validator)});
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

void StatemachineValidator::checkUniqueStatesAndEvents(
    const Statemachine &model,
    const pegium::validation::ValidationAcceptor &accept) const {
  std::unordered_map<std::string, std::vector<const Event *>> eventsByName;
  for (const auto &event : model.events) {
    if (event) {
      eventsByName[event->name].push_back(event.get());
    }
  }
  std::unordered_map<std::string, std::vector<const State *>> statesByName;
  for (const auto &state : model.states) {
    if (state) {
      statesByName[state->name].push_back(state.get());
    }
  }

  for (const auto &[name, events] : eventsByName) {
    if (events.size() <= 1) {
      continue;
    }
    for (const auto *event : events) {
      if (event != nullptr) {
        accept.error(*event, "Duplicate identifier name: " + name)
            .property<&Event::name>();
      }
    }
  }

  for (const auto &[name, states] : statesByName) {
    if (states.size() <= 1) {
      continue;
    }
    for (const auto *state : states) {
      if (state != nullptr) {
        accept.error(*state, "Duplicate identifier name: " + name)
            .property<&State::name>();
      }
    }
  }
}

} // namespace statemachine::services::validation
