#pragma once

#include <pegium/syntax-tree/AstNode.hpp>

namespace domainmodel::ast {

struct AbstractElement : pegium::AstNode {};
struct Type : AbstractElement {};

struct Feature : pegium::AstNode {
  bool many = false;
  string name;
  reference<Type> type;
};

struct DataType : Type {
  string name;
};

struct Entity : Type {
  string name;
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};

struct PackageDeclaration : AbstractElement {
  string name;
  vector<pointer<AbstractElement>> elements;
};

struct DomainModel : pegium::AstNode {
  vector<pointer<AbstractElement>> elements;
};

} // namespace domainmodel::ast
