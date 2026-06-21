#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace requirements::ast {

struct Contact : pegium::AstNode {
  string userName;
};

struct Environment : pegium::NamedAstNode {
  string description;
};

struct Requirement : pegium::NamedAstNode {
  string text;
  vector<reference<Environment>> environments;
};

struct RequirementModel : pegium::AstNode {
  pointer<Contact> contact;
  vector<pointer<Environment>> environments;
  vector<pointer<Requirement>> requirements;
};

struct Test : pegium::NamedAstNode {
  optional<string> testFile;
  vector<reference<Requirement>> requirements;
  vector<reference<Environment>> environments;
};

struct TestModel : pegium::AstNode {
  pointer<Contact> contact;
  vector<pointer<Test>> tests;
};

} // namespace requirements::ast
