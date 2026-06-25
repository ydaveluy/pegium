#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <arithmetics/core/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace arithmetics {

std::string canonical_identifier(std::string_view text);

using ContextValue = std::variant<double, const ast::Definition *>;
using EvaluationResult = std::unordered_map<const ast::Evaluation *, double>;

EvaluationResult interpret_evaluations(ast::Module &module);
std::vector<double> evaluate_module(ast::Module &module);

} // namespace arithmetics

namespace arithmetics::parser {

using namespace pegium::parser;

class ArithmeticParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Module;
  }

  const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  // terminal SL_COMMENT returns string_view: ('//' (!&('\n' | '\r\n' | '\r' | !.) .)* &('\n' | '\r\n' | '\r' | !.));
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  // terminal ML_COMMENT returns string_view: ('/*' (!'*/' .)* '*/');
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  // terminal ID returns string: ([A-Z_a-z] [0-9A-Z_a-z]*);
  Terminal<std::string> ID{
      "ID", "a-zA-Z_"_cr + many(w),
      opt::with_converter([](std::string_view text) noexcept {
        return canonical_identifier(text);
      })};
  // terminal NUMBER returns double: ([0-9]+ ('.' [0-9]*)?);
  Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};

  // Module returns Module: ('module'i name=ID statements+=Statement*);
  Rule<ast::Module> Module{
      "Module",
      "module"_kw.i() + assign<&ast::Module::name>(ID) +
          many(append<&ast::Module::statements>(Statement))};

  // Statement returns AstNode: (Definition | Evaluation);
  Rule<pegium::AstNode> Statement{"Statement", Definition | Evaluation};

  // Definition returns Definition: ('def'i name=ID ('(' args+=DeclaredParameter (',' args+=DeclaredParameter)* ')')? ':' expr=Expression ';');
  Rule<ast::Definition> Definition{
      "Definition",
      "def"_kw.i() + assign<&ast::Definition::name>(ID) +
          option("("_kw +
                 append<&ast::Definition::args>(DeclaredParameter) +
                 many(","_kw +
                      append<&ast::Definition::args>(DeclaredParameter)) +
                 ")"_kw) +
          ":"_kw + assign<&ast::Definition::expr>(Expression) + ";"_kw};

  // DeclaredParameter returns DeclaredParameter: name=ID;
  Rule<ast::DeclaredParameter> DeclaredParameter{
      "DeclaredParameter", assign<&ast::DeclaredParameter::name>(ID)};

  // Evaluation returns Evaluation: (expression=Expression ';');
  Rule<ast::Evaluation> Evaluation{
      "Evaluation",
      assign<&ast::Evaluation::expression>(Expression) + ";"_kw};

  // Expression returns Expression: BinaryExpression;
  Rule<ast::Expression> Expression{"Expression", BinaryExpression};

  // infix BinaryExpression returns BinaryExpression on PrimaryExpression: left assoc '%' > left assoc '^' > left assoc ('*' | '/') > left assoc ('+' | '-');
  InfixRule<ast::BinaryExpression, &ast::BinaryExpression::left,
            &ast::BinaryExpression::op,
            &ast::BinaryExpression::right>
      BinaryExpression{"BinaryExpression",
                       PrimaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};

  // PrimaryExpression returns Expression: (({GroupedExpression} '(' expression=Expression ')') | ({NumberLiteral} value=NUMBER) | ({FunctionCall} func=ID ('(' args+=Expression (',' args+=Expression)* ')')?));
  Rule<ast::Expression> PrimaryExpression{
      "PrimaryExpression",
      create<ast::GroupedExpression>() +
          "("_kw + assign<&ast::GroupedExpression::expression>(Expression) +
          ")"_kw |
          create<ast::NumberLiteral>() +
              assign<&ast::NumberLiteral::value>(NUMBER) |
          create<ast::FunctionCall>() +
              assign<&ast::FunctionCall::func>(ID) +
              option("("_kw +
                     append<&ast::FunctionCall::args>(Expression) +
                     many(","_kw +
                          append<&ast::FunctionCall::args>(Expression)) +
                     ")"_kw)};
#pragma clang diagnostic pop
};

} // namespace arithmetics::parser
