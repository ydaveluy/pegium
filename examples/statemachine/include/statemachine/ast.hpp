#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace statemachine::ast {

struct Event : pegium::NamedAstNode {};

struct Command : pegium::NamedAstNode {};

struct State;

struct Transition : pegium::AstNode {
  reference<Event> event;
  reference<State> state;
};

struct State : pegium::NamedAstNode {
  vector<reference<Command>> actions;
  vector<pointer<Transition>> transitions;
};

struct Statemachine : pegium::NamedAstNode {
  vector<pointer<Event>> events;
  vector<pointer<Command>> commands;
  reference<State> init;
  vector<pointer<State>> states;
};

} // namespace statemachine::ast
