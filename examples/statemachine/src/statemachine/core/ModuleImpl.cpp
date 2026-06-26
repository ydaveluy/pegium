#include <statemachine/core/ModuleImpl.hpp>
#include <statemachine/core/StateMachineParser.hpp>

namespace statemachine::parser {

std::unique_ptr<const pegium::parser::Parser>
makeStateMachineParser(const pegium::CoreServices &services) {
  return std::make_unique<const StateMachineParser>(services);
}

} // namespace statemachine::parser
