#pragma once

#include <string>
#include <string_view>

#include <pegium/LspTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>

namespace pegium::lsp::test_navigation {

using namespace pegium::parser;

struct NavigationEntry : AstNode {
  string name;
};

struct NavigationUse : AstNode {
  reference<NavigationEntry> target;
};

struct NavigationModel : AstNode {
  vector<pointer<NavigationEntry>> entries;
  vector<pointer<NavigationUse>> uses;
};

class NavigationParser final : public PegiumParser {
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
  static constexpr auto WS = some(s);
  Skipper skipper = SkipperBuilder().ignore(WS).build();

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<NavigationEntry> EntryRule{
      "Entry", "entry"_kw + assign<&NavigationEntry::name>(ID)};
  Rule<NavigationUse> UseRule{"Use",
                              "use"_kw + assign<&NavigationUse::target>(ID)};
  Rule<NavigationModel> ModelRule{
      "Model",
      some(append<&NavigationModel::entries>(EntryRule) |
           append<&NavigationModel::uses>(UseRule))};
#pragma clang diagnostic pop
};

inline const services::Services *
lookup_services(const services::SharedServices &shared,
                std::string_view languageId) {
  const auto *coreServices =
      shared.serviceRegistry->getServicesByLanguageId(languageId);
  if (coreServices == nullptr) {
    return nullptr;
  }
  return dynamic_cast<const services::Services *>(coreServices);
}

inline TextOffset use_name_offset(const workspace::Document &document) {
  return static_cast<TextOffset>(document.text().rfind("Alpha"));
}

inline ::lsp::LocationLink link_to_element(const AstNode &element) {
  const auto *document = tryGetDocument(element);
  const auto cstNode = element.getCstNode();

  ::lsp::LocationLink link{};
  link.targetUri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  link.targetRange.start = document->offsetToPosition(cstNode.getBegin());
  link.targetRange.end = document->offsetToPosition(cstNode.getEnd());
  link.targetSelectionRange = link.targetRange;
  return link;
}

} // namespace pegium::lsp::test_navigation
