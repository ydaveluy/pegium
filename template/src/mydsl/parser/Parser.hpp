#pragma once

#include <mydsl/ast.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

// This file is the grammar of your language. Edit the rules below to make
// the language your own; the AST in `mydsl/ast.hpp` and this grammar evolve
// together.
//
// Equivalent Langium grammar shipped as the default:
//
//   entry Model:
//       (persons+=Person | greetings+=Greeting)*;
//
//   Person:  'person' name=ID;
//   Greeting: 'Hello' person=[Person:ID] '!';
//
//   hidden terminal WS:         /\s+/;
//   terminal ID:                /[_a-zA-Z][\w_]*/;
//   hidden terminal ML_COMMENT: /\/\*[\s\S]*?\*\//;
//   hidden terminal SL_COMMENT: /\/\/[^\n\r]*/;

namespace mydsl::parser {

using namespace pegium::parser;

class MyDslParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return ModelRule;
  }
  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

  // Hidden terminals: whitespace and comments. They never appear in the AST
  // but `hidden(...)` keeps comments available in the CST so formatters and
  // hover providers can still see them.
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  // ID matches /[_a-zA-Z][\w_]*/.
  Terminal<std::string> ID{"ID", "_a-zA-Z"_cr + many(w)};

  // Person: 'person' name=ID ;
  Rule<ast::Person> PersonRule{
      "Person",
      "person"_kw + assign<&ast::Person::name>(ID)};

  // Greeting: 'Hello' person=[Person:ID] '!' ;
  // The captured ID text is stored in `Greeting::person`. Because the field
  // type is `reference<Person>`, the linker resolves it to a Person target
  // after parsing finishes.
  Rule<ast::Greeting> GreetingRule{
      "Greeting",
      "Hello"_kw + assign<&ast::Greeting::person>(ID) + "!"_kw};

  // Model: (Person | Greeting)+ ;
  // Pegium rule bodies must be non-nullable. The Langium HelloWorld used
  // `(...)*` which matches the empty file too; here we use `some(...)`
  // (one-or-more) so the grammar requires at least one declaration.
  // The two rules feed the same polymorphic vector, so a single `append`
  // takes the ordered choice directly.
  Rule<ast::Model> ModelRule{
      "Model",
      some(append<&ast::Model::elements>(PersonRule | GreetingRule))};

#pragma clang diagnostic pop
};

} // namespace mydsl::parser
