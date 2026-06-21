#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <statemachine/core/ast.hpp>

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
  // terminal SL_COMMENT returns string_view: ('//' (!&('\n' | '\r\n' | '\r' | !.) .)* &('\n' | '\r\n' | '\r' | !.));
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  // terminal ML_COMMENT returns string_view: ('/*' (!'*/' .)* '*/');
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  // terminal ID returns string: ([A-Z_a-z] [0-9A-Z_a-z]*);
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  // ReservedKeywords returns string: ('statemachine'i | 'events'i | 'commands'i | 'initialstate'i | 'state'i | 'actions'i | 'end'i);
  Rule<std::string> ReservedKeywords{
      "ReservedKeywords",
      "statemachine"_kw.i() | "events"_kw.i() | "commands"_kw.i() |
          "initialState"_kw.i() | "state"_kw.i() | "actions"_kw.i() |
          "end"_kw.i()};

  // ValidID returns string: (!ReservedKeywords ID);
  Rule<std::string> ValidID{"ValidID", !ReservedKeywords + ID};

  // Event returns Event: name=ValidID;
  Rule<ast::Event> EventRule{"Event", assign<&ast::Event::name>(ValidID)};

  // Command returns Command: name=ValidID;
  Rule<ast::Command> CommandRule{"Command",
                                 assign<&ast::Command::name>(ValidID)};

  // Transition returns Transition: (event=ValidID '=>' state=ValidID);
  Rule<ast::Transition> TransitionRule{
      "Transition", assign<&ast::Transition::event>(ValidID) + "=>"_kw +
                        assign<&ast::Transition::state>(ValidID)};

  // State returns State: ('state'i name=ValidID ('actions'i '{' actions+=ValidID+ '}')? transitions+=Transition* 'end'i);
  Rule<ast::State> StateRule{
      "State",
      "state"_kw.i() + assign<&ast::State::name>(ValidID) +
          option("actions"_kw.i() + "{"_kw +
                 some(append<&ast::State::actions>(ValidID)) + "}"_kw) +
          many(append<&ast::State::transitions>(TransitionRule)) +
          "end"_kw.i()};

  // Statemachine returns Statemachine: ('statemachine'i name=ValidID ('events'i events+=Event+)? ('commands'i commands+=Command+)? 'initialstate'i init=ValidID states+=State*);
  Rule<ast::Statemachine> StatemachineRule{
      "Statemachine",
      "statemachine"_kw.i() + assign<&ast::Statemachine::name>(ValidID) +
          option("events"_kw.i() +
                 some(append<&ast::Statemachine::events>(EventRule))) +
          option("commands"_kw.i() +
                 some(append<&ast::Statemachine::commands>(CommandRule))) +
          "initialState"_kw.i() + assign<&ast::Statemachine::init>(ValidID) +
          many(append<&ast::Statemachine::states>(StateRule))};
#pragma clang diagnostic pop
};

} // namespace statemachine::parser
