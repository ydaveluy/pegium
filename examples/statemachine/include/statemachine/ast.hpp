#pragma once

#include <pegium/syntax-tree/AstNode.hpp>

namespace statemachine::ast {

struct Event : pegium::AstNode {
  string name;
};

struct Command : pegium::AstNode {
  string name;
};

struct State;

struct Transition : pegium::AstNode {
  reference<Event> event;
  reference<State> state;
};

struct State : pegium::AstNode {
  string name;
  vector<reference<Command>> actions;
  vector<pointer<Transition>> transitions;
};

struct Statemachine : pegium::AstNode {
  string name;
  vector<pointer<Event>> events;
  vector<pointer<Command>> commands;
  reference<State> init;
  vector<pointer<State>> states;
};

} // namespace statemachine::ast
