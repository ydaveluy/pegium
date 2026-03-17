#include <gtest/gtest.h>

#include <typeindex>

#include <pegium/LspTestSupport.hpp>
#include <pegium/references/References.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/workspace/AstDescriptions.hpp>

namespace pegium::lsp {
namespace {

struct DefinitionType {};

class TestReferences final : public references::References {
public:
  std::optional<workspace::AstNodeDescription> declaration;
  std::vector<workspace::ReferenceDescription> references;

  std::optional<workspace::AstNodeDescription>
  findDeclarationAt(const workspace::Document &, TextOffset) const override {
    return declaration;
  }

  utils::stream<workspace::ReferenceDescriptionOrDeclaration>
  findReferencesAt(const workspace::Document &, TextOffset,
                   bool includeDeclaration) const override {
    std::vector<workspace::ReferenceDescriptionOrDeclaration> results;
    if (includeDeclaration && declaration.has_value()) {
      results.emplace_back(*declaration);
    }
    for (const auto &reference : references) {
      results.emplace_back(reference);
    }
    return utils::make_stream<workspace::ReferenceDescriptionOrDeclaration>(
        std::move(results));
  }
};

TEST(DefaultDocumentHighlightProviderTest,
     HighlightsDeclarationAndLocalReferences) {
  auto shared = test::make_shared_services();
  auto services = test::make_services(*shared, "test", {".test"});

  auto references = std::make_unique<TestReferences>();
  references->declaration = workspace::AstNodeDescription{
      .name = "value",
      .documentId = 1,
      .offset = 0,
      .nameLength = 5,
  };
  references->references.push_back(workspace::ReferenceDescription{
      .sourceDocumentId = 1,
      .sourceOffset = 6,
      .sourceLength = 5,
      .referenceType = std::type_index(typeid(DefinitionType)),
  });
  services->references.references = std::move(references);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  workspace::Document document;
  document.id = 1;
  document.uri = test::make_file_uri("highlights.test");
  document.languageId = "test";
  document.setText("value value");

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("test");
  ASSERT_NE(coreServices, nullptr);
  const auto *fullServices = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(fullServices, nullptr);

  ::lsp::DocumentHighlightParams params{};
  params.position.line = 0;
  params.position.character = 7;

  const auto highlights =
      fullServices->lsp.documentHighlightProvider->getDocumentHighlight(document,
                                                                       params);
  ASSERT_EQ(highlights.size(), 2u);
  EXPECT_EQ(highlights[0].kind, ::lsp::DocumentHighlightKind::Write);
  EXPECT_EQ(highlights[0].range.start.character, 0u);
  EXPECT_EQ(highlights[1].kind, ::lsp::DocumentHighlightKind::Read);
  EXPECT_EQ(highlights[1].range.start.character, 6u);
}

} // namespace
} // namespace pegium::lsp
