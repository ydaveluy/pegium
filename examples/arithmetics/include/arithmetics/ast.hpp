#pragma once

#include <pegium/syntax-tree/AstNode.hpp>

namespace arithmetics::ast {

struct Module;
struct Expression : pegium::AstNode {};
struct AbstractDefinition : pegium::AstNode {};

struct DeclaredParameter : AbstractDefinition {
  string name;
};

struct Definition : AbstractDefinition {
  string name;
  vector<pointer<DeclaredParameter>> args;
  pointer<Expression> expr;
};

struct Evaluation : pegium::AstNode {
  pointer<Expression> expression;
};

struct Module : pegium::AstNode {
  string name;
  vector<pointer<pegium::AstNode>> statements;
};

struct BinaryExpression : Expression {
  pointer<Expression> left;
  string op;
  pointer<Expression> right;
};

struct GroupedExpression : Expression {
  pointer<Expression> expression;
};

struct NumberLiteral : Expression {
  double value = 0.0;
};

struct FunctionCall : Expression {
  reference<AbstractDefinition> func;
  vector<pointer<Expression>> args;
};

} // namespace arithmetics::ast
