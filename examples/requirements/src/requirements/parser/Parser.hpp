#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <requirements/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace requirements {

std::string decode_quoted_string(std::string_view text);

} // namespace requirements

namespace requirements::parser {

using namespace pegium::parser;

class RequirementsParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

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
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Terminal<std::string> STRING{
      "STRING",
      ("\""_kw + many("\\"_kw + dot | !"\""_kw + dot) + "\""_kw) |
          ("'"_kw + many("\\"_kw + dot | !"'"_kw + dot) + "'"_kw),
      opt::with_converter(
          [](std::string_view text) noexcept
              -> opt::ConversionResult<std::string> {
            return opt::conversion_value<std::string>(
                decode_quoted_string(text));
          })};

  Rule<ast::Contact> ContactRule{
      "Contact",
      "contact"_kw.i() + ":"_kw + assign<&ast::Contact::userName>(STRING)};

  Rule<ast::Environment> EnvironmentRule{
      "Environment",
      "environment"_kw.i() + assign<&ast::Environment::name>(ID) + ":"_kw +
          assign<&ast::Environment::description>(STRING)};

  Rule<ast::Requirement> RequirementRule{
      "Requirement",
      "req"_kw.i() + assign<&ast::Requirement::name>(ID) +
          assign<&ast::Requirement::text>(STRING) +
          option("applicable"_kw.i() + "for"_kw.i() +
                 append<&ast::Requirement::environments>(ID) +
                 many(","_kw + append<&ast::Requirement::environments>(ID)))};

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
  using PegiumParser::parse;

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
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Terminal<std::string> STRING{
      "STRING",
      ("\""_kw + many("\\"_kw + dot | !"\""_kw + dot) + "\""_kw) |
          ("'"_kw + many("\\"_kw + dot | !"'"_kw + dot) + "'"_kw),
      opt::with_converter(
          [](std::string_view text) noexcept
              -> opt::ConversionResult<std::string> {
            return opt::conversion_value<std::string>(
                decode_quoted_string(text));
          })};

  Rule<ast::Contact> ContactRule{
      "Contact",
      "contact"_kw.i() + ":"_kw + assign<&ast::Contact::userName>(STRING)};

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

  Rule<ast::TestModel> TestModelRule{
      "TestModel",
      option(assign<&ast::TestModel::contact>(ContactRule)) +
          some(append<&ast::TestModel::tests>(TestRule))};
#pragma clang diagnostic pop
};

} // namespace requirements::parser
