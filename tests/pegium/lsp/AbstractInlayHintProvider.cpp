#include <gtest/gtest.h>

#include <string>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/AbstractInlayHintProvider.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>

namespace pegium::lsp {
namespace {

using namespace pegium::parser;

struct HintEntry : AstNode {
  string name;
};

struct HintModel : AstNode {
  vector<pointer<HintEntry>> entries;
};

class HintParser final : public PegiumParser {
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
  Rule<HintEntry> EntryRule{"Entry", "entry"_kw + assign<&HintEntry::name>(ID)};
  Rule<HintModel> ModelRule{"Model", some(append<&HintModel::entries>(EntryRule))};
#pragma clang diagnostic pop
};

const services::Services *lookup_services(const services::SharedServices &shared,
                                          std::string_view languageId) {
  const auto *coreServices =
      shared.serviceRegistry->getServicesByLanguageId(languageId);
  if (coreServices == nullptr) {
    return nullptr;
  }
  return dynamic_cast<const services::Services *>(coreServices);
}

class TestInlayHintProvider final : public AbstractInlayHintProvider {
public:
  using AbstractInlayHintProvider::AbstractInlayHintProvider;

  mutable std::vector<std::string> visitedNames;

protected:
  void computeInlayHint(const AstNode &astNode,
                        const InlayHintAcceptor &acceptor) const override {
    const auto *entry = dynamic_cast<const HintEntry *>(&astNode);
    if (entry == nullptr) {
      return;
    }

    visitedNames.push_back(entry->name);

    ::lsp::InlayHint hint{};
    hint.position = getDocument(astNode).offsetToPosition(astNode.getCstNode().getBegin());
    hint.label = entry->name;
    acceptor(std::move(hint));
  }
};

TEST(AbstractInlayHintProviderTest, VisitsOnlyAstNodesInRequestedRange) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<HintParser>(*shared, "hint", {".hint"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("hint.hint"), "hint",
      "entry Alpha\n"
      "entry Beta");
  ASSERT_NE(document, nullptr);

  const auto *services = lookup_services(*shared, "hint");
  ASSERT_NE(services, nullptr);

  TestInlayHintProvider provider(*services);

  ::lsp::InlayHintParams params{};
  params.range.start = document->offsetToPosition(
      static_cast<TextOffset>(document->text().find("entry Beta")));
  params.range.end = document->offsetToPosition(
      static_cast<TextOffset>(document->text().size()));

  const auto hints =
      provider.getInlayHints(*document, params, utils::default_cancel_token);
  ASSERT_EQ(hints.size(), 1u);
  EXPECT_EQ(provider.visitedNames, std::vector<std::string>{"Beta"});
  ASSERT_TRUE(std::holds_alternative<::lsp::String>(hints[0].label));
  EXPECT_EQ(std::get<::lsp::String>(hints[0].label), "Beta");
}

} // namespace
} // namespace pegium::lsp
