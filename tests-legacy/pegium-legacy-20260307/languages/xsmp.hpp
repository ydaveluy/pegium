#pragma once
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/syntax-tree/AstNode.hpp>

namespace Xsmp {
struct Type;

struct Expression : pegium::AstNode {};
struct Attribute : public pegium::AstNode {
  reference<Type> type;

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
  vector<reference<Type>> bases;
  vector<pointer<NamedElement>> members;
};
struct Component : public Type {
  optional<reference<Type>> base;
  vector<reference<Type>> interfaces;
  vector<pointer<NamedElement>> members;
};

struct Model : public Component {};
struct Service : public Component {};

struct Array : public Type {
  reference<Type> itemType;
  pointer<Expression> size;
};

struct ValueReference : public Type {
  reference<Type> type;
};
struct Integer : public Type {
  optional<reference<Type>> primitiveType;
  pointer<Expression> minimum;
  pointer<Expression> maximum;
};
struct Float : public Type {
  optional<reference<Type>> primitiveType;
  pointer<Expression> minimum;
  pointer<Expression> maximum;
  string kind;
};
struct EventType : public Type {
  reference<Type> eventArg;
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
  reference<Type> type;
  pointer<Expression> value;
};

struct Field : public VisibilityElement {
  reference<Type> type;
  pointer<Expression> value;
};

struct Property : public VisibilityElement {
  reference<Type> type;
  // TODO add attached field + throws
};
struct Association : public VisibilityElement {
  reference<Type> type;
};

struct Container : public NamedElement {
  reference<Type> type;
};

struct Reference : public NamedElement {
  reference<Type> type;
};

struct AttributeType : public Type {
  reference<Type> type;
  pointer<Expression> value;
};
struct EnumerationLiteral : public NamedElement {
  pointer<Expression> value;
};
struct Enumeration : public Type {
  vector<pointer<EnumerationLiteral>> literals;
};
} // namespace Xsmp
