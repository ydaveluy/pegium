#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <statemachine/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace statemachine::parser {

using namespace pegium::parser;

class StateMachineParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return StatemachineRule;
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

  Rule<ast::Event> EventRule{"Event", assign<&ast::Event::name>(ID)};

  Rule<ast::Command> CommandRule{"Command", assign<&ast::Command::name>(ID)};

  Rule<ast::Transition> TransitionRule{
      "Transition", assign<&ast::Transition::event>(ID) + "=>"_kw +
                        assign<&ast::Transition::state>(ID)};

  Rule<ast::State> StateRule{
      "State",
      "state"_kw.i() + assign<&ast::State::name>(ID) +
          option("actions"_kw.i() + "{"_kw +
                 some(append<&ast::State::actions>(ID)) + "}"_kw) +
          many(append<&ast::State::transitions>(TransitionRule)) +
          "end"_kw.i()};

  Rule<ast::Statemachine> StatemachineRule{
      "Statemachine",
      "statemachine"_kw.i() + assign<&ast::Statemachine::name>(ID) +
          option("events"_kw.i() + append<&ast::Statemachine::events>(EventRule) +
                 many(!("commands"_kw.i() | "initialState"_kw.i()) +
                      append<&ast::Statemachine::events>(EventRule))) +
          option("commands"_kw.i() +
                 append<&ast::Statemachine::commands>(CommandRule) +
                 many(!"initialState"_kw.i() +
                      append<&ast::Statemachine::commands>(CommandRule))) +
          "initialState"_kw.i() + assign<&ast::Statemachine::init>(ID) +
          many(append<&ast::Statemachine::states>(StateRule))};
#pragma clang diagnostic pop
};

} // namespace statemachine::parser
