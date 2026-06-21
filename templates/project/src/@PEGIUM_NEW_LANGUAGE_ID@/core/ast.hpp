#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@::ast {

struct Person : pegium::NamedAstNode {};

struct Greeting : pegium::AstNode {
  reference<Person> person;
};

struct Model : pegium::AstNode {
  vector<pointer<Person>> persons;
  vector<pointer<Greeting>> greetings;
};

} // namespace @PEGIUM_NEW_LANGUAGE_ID@::ast
