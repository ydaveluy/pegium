#pragma once

#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/ParseSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::test {

class RuleParser final : public parser::PegiumParser {
public:
  RuleParser(const grammar::ParserRule &entryRule, parser::Skipper skipper,
             parser::ParseOptions options = {}) noexcept
      : _entryRule(entryRule), _skipper(std::move(skipper)), _options(options) {}

protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return _entryRule;
  }

  const parser::Skipper &getSkipper() const noexcept override {
    return _skipper;
  }

  parser::ParseOptions getParseOptions() const noexcept override {
    return _options;
  }

private:
  const grammar::ParserRule &_entryRule;
  parser::Skipper _skipper;
  parser::ParseOptions _options;
};

inline void parse_rule(const grammar::ParserRule &entryRule,
                       workspace::Document &document,
                       const parser::Skipper &skipper,
                       const parser::ParseOptions &options = {}) {
  RuleParser parser(entryRule, skipper, options);
  apply_parse_result(document,
                     parser.parse(document.textDocument().getText()));
}

[[nodiscard]] inline parser::ParseResult
parse_rule_result(const grammar::ParserRule &entryRule, std::string_view text,
                  const parser::Skipper &skipper,
                  const parser::ParseOptions &options = {}) {
  RuleParser parser(entryRule, skipper, options);
  return parser.parse(text);
}

} // namespace pegium::test
