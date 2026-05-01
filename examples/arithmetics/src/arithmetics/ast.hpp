#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace arithmetics::ast {

struct Module;
struct Expression : pegium::AstNode {};
struct AbstractDefinition : pegium::NamedAstNode {};

struct DeclaredParameter : AbstractDefinition {};

struct Definition : AbstractDefinition {
  vector<pointer<DeclaredParameter>> args;
  pointer<Expression> expr;
};

struct Evaluation : pegium::AstNode {
  pointer<Expression> expression;
};

struct Module : pegium::NamedAstNode {
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
