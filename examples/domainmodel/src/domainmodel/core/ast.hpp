#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace domainmodel::ast {

struct AbstractElement : pegium::NamedAstNode {};
struct Type : AbstractElement {};

struct Feature : pegium::NamedAstNode {
  bool many = false;
  reference<Type> type;
};

struct DataType : Type {};

struct Entity : Type {
  optional<reference<Entity>> superType;
  vector<pointer<Feature>> features;
};

struct PackageDeclaration : AbstractElement {
  vector<pointer<AbstractElement>> elements;
};

struct DomainModel : pegium::AstNode {
  vector<pointer<AbstractElement>> elements;
};

} // namespace domainmodel::ast
