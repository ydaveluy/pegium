#pragma once
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/syntax-tree.hpp>

namespace Xsmp {
struct Type;

struct Expression : pegium::AstNode {};
struct Attribute : public pegium::AstNode {
  /*reference<Type>*/ string type;

  pointer<Expression> value;
};
struct NamedElement : public pegium::AstNode {
  string name;
  vector<pointer<Attribute>> attributes;
};

struct VisibilityElement : public NamedElement {
  vector<string> modifiers;
};
struct Namespace;
struct Catalogue : public NamedElement {
  vector<pointer<Namespace>> namespaces;
};

struct Namespace : public NamedElement {
  vector<pointer<NamedElement>> members;
};

struct Type : public VisibilityElement {};
struct Structure : public Type {
  vector<pointer<NamedElement>> members;
};

struct Class : public Structure {};
struct Exception : public Class {};

struct Interface : public Type {
  vector<string> bases;
  vector<pointer<NamedElement>> members;
};
struct Component : public Type {
  optional<string> base;
  vector<string> interfaces;
  vector<pointer<NamedElement>> members;
};

struct Model : public Component {};
struct Service : public Component {};

struct Array : public Type {
  string itemType;
  pointer<Expression> size;
};

struct ValueReference : public Type {
  string type;
};
struct Integer : public Type {
  string primitiveType;
  pointer<Expression> minimum;
  pointer<Expression> maximum;
};
struct Float : public Type {
  string primitiveType;
  pointer<Expression> minimum;
  pointer<Expression> maximum;
  string kind;
};
struct EventType : public Type {
  string eventArg;
};
struct StringType : public Type {
  pointer<Expression> size;
};
struct PrimitiveType : public Type {};
struct NativeType : public Type {};
struct BinaryExpression : Expression {
  pointer<Expression> leftOperand;
  string feature;
  pointer<Expression> rightOperand;
};

struct BooleanLiteral : Expression {
  bool isTrue;
};
struct Constant : public VisibilityElement {
  string type;
  pointer<Expression> value;
};

struct Field : public VisibilityElement {
  string type;
  pointer<Expression> value;
};

struct Property : public VisibilityElement {
  string type;
  // TODO add attached field + throws
};
struct Association : public VisibilityElement {
  string type;
};

struct Container : public NamedElement {
  string type;
};

struct Reference : public NamedElement {
  string type;
};

struct AttributeType : public Type {
  string type;
  pointer<Expression> value;
};
struct EnumerationLiteral : public NamedElement {
  pointer<Expression> value;
};
struct Enumeration : public Type {
  vector<pointer<EnumerationLiteral>> literals;
};
} // namespace Xsmp
