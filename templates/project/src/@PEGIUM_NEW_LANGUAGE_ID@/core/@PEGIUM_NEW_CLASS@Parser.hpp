#pragma once

#include <string>

#include <@PEGIUM_NEW_LANGUAGE_ID@/core/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>

namespace @PEGIUM_NEW_LANGUAGE_ID@::parser {

using namespace pegium::parser;

class @PEGIUM_NEW_CLASS@Parser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return ModelRule;
  }
  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  // terminal ID returns string: ([A-Z_a-z] [0-9A-Z_a-z]*);
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  // Person: 'person' name=ID;
  Rule<ast::Person> PersonRule{"Person",
                               "person"_kw + assign<&ast::Person::name>(ID)};

  // Greeting: 'Hello' person=[Person:ID] '!';
  Rule<ast::Greeting> GreetingRule{
      "Greeting", "Hello"_kw + assign<&ast::Greeting::person>(ID) + "!"_kw};

  // Model: (persons+=Person | greetings+=Greeting)*;
  NullableRule<ast::Model> ModelRule{
      "Model", many(append<&ast::Model::persons>(PersonRule) |
                    append<&ast::Model::greetings>(GreetingRule))};
#pragma clang diagnostic pop
};

} // namespace @PEGIUM_NEW_LANGUAGE_ID@::parser
