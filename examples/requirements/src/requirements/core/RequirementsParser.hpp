#pragma once

#include <requirements/core/ast.hpp>
#include <requirements/core/Language.hpp>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace requirements::parser {

using namespace pegium::parser;

class RequirementsParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return RequirementModelRule;
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
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  // terminal STRING returns string: (('\"' (('\\' .) | (!'\"' .))* '\"') | ('\'' (('\\' .) | (!'\'' .))* '\''));
  Terminal<std::string> STRING{
      "STRING",
      ("\""_kw + many("\\"_kw + dot | !"\""_kw + dot) + "\""_kw) |
          ("'"_kw + many("\\"_kw + dot | !"'"_kw + dot) + "'"_kw),
      opt::with_converter([](std::string_view text) noexcept {
        return decode_quoted_string(text);
      })};

  // Contact returns Contact: ('contact'i ':' userName=STRING);
  Rule<ast::Contact> ContactRule{
      "Contact",
      "contact"_kw.i() + ":"_kw + assign<&ast::Contact::userName>(STRING)};

  // Environment returns Environment: ('environment'i name=ID ':' description=STRING);
  Rule<ast::Environment> EnvironmentRule{
      "Environment",
      "environment"_kw.i() + assign<&ast::Environment::name>(ID) + ":"_kw +
          assign<&ast::Environment::description>(STRING)};

  // Requirement returns Requirement: ('req'i name=ID text=STRING ('applicable'i 'for'i environments+=ID (',' environments+=ID)*)?);
  Rule<ast::Requirement> RequirementRule{
      "Requirement",
      "req"_kw.i() + assign<&ast::Requirement::name>(ID) +
          assign<&ast::Requirement::text>(STRING) +
          option("applicable"_kw.i() + "for"_kw.i() +
                 append<&ast::Requirement::environments>(ID) +
                 many(","_kw + append<&ast::Requirement::environments>(ID)))};

  // RequirementModel returns RequirementModel: (contact=Contact? environments+=Environment* requirements+=Requirement+);
  Rule<ast::RequirementModel> RequirementModelRule{
      "RequirementModel",
      option(assign<&ast::RequirementModel::contact>(ContactRule)) +
          many(append<&ast::RequirementModel::environments>(EnvironmentRule)) +
          some(append<&ast::RequirementModel::requirements>(RequirementRule))};
#pragma clang diagnostic pop
};

class TestsParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return TestModelRule;
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
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  // terminal STRING returns string: (('\"' (('\\' .) | (!'\"' .))* '\"') | ('\'' (('\\' .) | (!'\'' .))* '\''));
  Terminal<std::string> STRING{
      "STRING",
      ("\""_kw + many("\\"_kw + dot | !"\""_kw + dot) + "\""_kw) |
          ("'"_kw + many("\\"_kw + dot | !"'"_kw + dot) + "'"_kw),
      opt::with_converter([](std::string_view text) noexcept {
        return decode_quoted_string(text);
      })};

  // Contact returns Contact: ('contact'i ':' userName=STRING);
  Rule<ast::Contact> ContactRule{
      "Contact",
      "contact"_kw.i() + ":"_kw + assign<&ast::Contact::userName>(STRING)};

  // Test returns Test: ('tst'i name=ID ('testfile'i '=' testFile=STRING)? 'tests'i requirements+=ID (',' requirements+=ID)* ('applicable'i 'for'i environments+=ID (',' environments+=ID)*)?);
  Rule<ast::Test> TestRule{
      "Test",
      "tst"_kw.i() + assign<&ast::Test::name>(ID) +
          option("testfile"_kw.i() + "="_kw +
                 assign<&ast::Test::testFile>(STRING)) +
          "tests"_kw.i() + append<&ast::Test::requirements>(ID) +
          many(","_kw + append<&ast::Test::requirements>(ID)) +
          option("applicable"_kw.i() + "for"_kw.i() +
                 append<&ast::Test::environments>(ID) +
                 many(","_kw + append<&ast::Test::environments>(ID)))};

  // TestModel returns TestModel: (contact=Contact? tests+=Test+);
  Rule<ast::TestModel> TestModelRule{
      "TestModel",
      option(assign<&ast::TestModel::contact>(ContactRule)) +
          some(append<&ast::TestModel::tests>(TestRule))};
#pragma clang diagnostic pop
};

} // namespace requirements::parser
