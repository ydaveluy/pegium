#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

// AST nodes for the default "Hello world" language.
//
// This file is the semantic model of your language. Add fields here, add
// matching rules in `parser/Parser.hpp`, and validation/scoping/formatting
// will line up automatically.
//
// Conventions used by Pegium:
//   - `pegium::NamedAstNode` provides a `name` field and is what the default
//     name provider uses to expose declarations to the index.
//   - `reference<T>` is a cross-reference resolved by the linker after parse.
//   - `pointer<T>` is an owned child node.
//   - `vector<...>` is a `std::vector<...>` (alias inherited from AstNode).

namespace mydsl::ast {

struct Person : pegium::NamedAstNode {};

struct Greeting : pegium::AstNode {
  reference<Person> person;
};

// The top-level Model holds a heterogeneous list of declarations and
// greetings, in source order. Pegium follows the same idiom as the
// arithmetics example: a single polymorphic vector + a wrapper rule that
// chooses one concrete type per slot. To split them by kind, use
// `dynamic_cast` (see `cli_main.cpp` and `tests/ParseSmoke.cpp`).
struct Model : pegium::AstNode {
  vector<pointer<pegium::AstNode>> elements;
};

} // namespace mydsl::ast
