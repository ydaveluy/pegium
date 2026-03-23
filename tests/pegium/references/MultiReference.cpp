#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

namespace pegium::references {
namespace {

using namespace pegium::parser;

struct Person : AstNode {
  string name;
};

struct Greeting : AstNode {
  multi_reference<Person> person;
};

struct Model : AstNode {
  vector<pointer<Person>> persons;
  vector<pointer<Greeting>> greetings;
};

class MultiReferenceParser final : public PegiumParser {
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

  Rule<Person> PersonRule{"Person",
                          "person"_kw + assign<&Person::name>(ID)};

  Rule<Greeting> GreetingRule{"Greeting",
                              "hello"_kw + assign<&Greeting::person>(ID)};

  Rule<Model> ModelRule{
      "Model",
      some(append<&Model::persons>(PersonRule) |
           append<&Model::greetings>(GreetingRule))};
#pragma clang diagnostic pop
};

bool has_diagnostic_message(const workspace::Document &document,
                            std::string_view needle) {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool register_language(services::SharedCoreServices &sharedServices) {
  auto services = test::make_uninstalled_core_services<MultiReferenceParser>(
      sharedServices, "multi-ref", {".mr"});
  pegium::services::installDefaultCoreServices(*services);
  sharedServices.serviceRegistry->registerServices(std::move(services));
  return true;
}

std::shared_ptr<workspace::Document>
open_and_build(services::SharedCoreServices &sharedServices,
               std::string_view fileName, std::string text) {
  return test::open_and_build_document(sharedServices, test::make_file_uri(fileName),
                                       "multi-ref", std::move(text));
}

TEST(MultiReferenceTest, ResolvesAllMatchingDeclarationsInSameDocument) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  ASSERT_TRUE(register_language(*shared));

  auto document = open_and_build(
      *shared, "same-document.mr",
      "person Alice\n"
      "person Bob\n"
      "person Alice\n"
      "hello Alice\n");

  ASSERT_NE(document, nullptr);
  auto *model = dynamic_cast<Model *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->persons.size(), 3u);
  ASSERT_EQ(model->greetings.size(), 1u);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.workspace.astNodeLocator, nullptr);
  ASSERT_NE(services.workspace.referenceDescriptionProvider, nullptr);
  const auto &locator = *services.workspace.astNodeLocator;

  const auto &reference = model->greetings.front()->person;
  EXPECT_EQ(reference.getRefText(), "Alice");
  EXPECT_FALSE(reference.hasError());
  ASSERT_EQ(reference.size(), 2u);
  ASSERT_NE(reference.data(), nullptr);
  ASSERT_NE(reference.front(), nullptr);
  ASSERT_NE(reference.back(), nullptr);
  EXPECT_EQ(reference.front()->name, "Alice");
  EXPECT_EQ(reference.back()->name, "Alice");
  EXPECT_EQ(reference[0]->name, "Alice");
  EXPECT_EQ(reference[1]->name, "Alice");
  EXPECT_TRUE(reference);

  std::unordered_set<std::string> targetPaths;
  for (const auto *target : reference) {
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->name, "Alice");
    targetPaths.insert(locator.getAstNodePath(*target));
  }
  EXPECT_EQ(targetPaths.size(), 2u);

  const auto descriptions =
      services.workspace.referenceDescriptionProvider->createDescriptions(
          *document);
  ASSERT_EQ(descriptions.size(), 2u);
  for (const auto &description : descriptions) {
    EXPECT_EQ(description.sourceText(document->textDocument().getText()),
              "Alice");
    EXPECT_TRUE(description.isResolved());
    ASSERT_TRUE(description.targetDocumentId.has_value());
    EXPECT_EQ(*description.targetDocumentId, document->id);
  }
}

TEST(MultiReferenceTest, ResolvesAllMatchingDeclarationsAcrossDocuments) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  ASSERT_TRUE(register_language(*shared));

  auto firstDocument =
      open_and_build(*shared, "first.mr", "person Alice\n");
  ASSERT_NE(firstDocument, nullptr);

  auto secondDocument = open_and_build(
      *shared, "second.mr",
      "person Alice\n"
      "person Bob\n"
      "person Alice\n"
      "hello Alice\n");
  ASSERT_NE(secondDocument, nullptr);

  auto *model = dynamic_cast<Model *>(secondDocument->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  const auto &reference = model->greetings.front()->person;
  ASSERT_EQ(reference.size(), 3u);

  const auto &services =
      shared->serviceRegistry->getServices(secondDocument->uri);
  ASSERT_NE(services.workspace.referenceDescriptionProvider, nullptr);
  const auto descriptions =
      services.workspace.referenceDescriptionProvider->createDescriptions(
          *secondDocument);
  std::unordered_map<workspace::DocumentId, std::size_t> targetsByDocumentId;
  for (const auto &description : descriptions) {
    ASSERT_TRUE(description.targetDocumentId.has_value());
    ++targetsByDocumentId[*description.targetDocumentId];
  }

  EXPECT_EQ(targetsByDocumentId[firstDocument->id], 1u);
  EXPECT_EQ(targetsByDocumentId[secondDocument->id], 2u);
}

TEST(MultiReferenceTest, IndexesOneLogicalReferenceAgainstEachResolvedTarget) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  ASSERT_TRUE(register_language(*shared));

  auto document = open_and_build(
      *shared, "indexed.mr",
      "person Alice\n"
      "person Bob\n"
      "person Alice\n"
      "hello Alice\n");
  ASSERT_NE(document, nullptr);

  auto *model = dynamic_cast<Model *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto firstAliceKey = workspace::NodeKey{
      .documentId = document->id,
      .symbolId = document->makeSymbolId(*model->persons[0])};
  const auto secondAliceKey = workspace::NodeKey{
      .documentId = document->id,
      .symbolId = document->makeSymbolId(*model->persons[2])};

  auto firstReferences =
      shared->workspace.indexManager->findAllReferences(firstAliceKey);
  auto secondReferences =
      shared->workspace.indexManager->findAllReferences(secondAliceKey);

  ASSERT_EQ(firstReferences.size(), 1u);
  ASSERT_EQ(secondReferences.size(), 1u);
}

TEST(MultiReferenceTest,
     ReferencesServiceIncludesSharedUsageForEachMatchingDeclaration) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  ASSERT_TRUE(register_language(*shared));

  auto document = open_and_build(
      *shared, "references-service.mr",
      "person Alice\n"
      "person Bob\n"
      "person Alice\n"
      "hello Alice\n");
  ASSERT_NE(document, nullptr);

  auto *model = dynamic_cast<Model *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.references.references, nullptr);
  ASSERT_NE(services.references.nameProvider, nullptr);

  const auto secondAliceName =
      services.references.nameProvider->getName(*model->persons[2]);
  ASSERT_TRUE(secondAliceName.has_value());
  EXPECT_EQ(*secondAliceName, "Alice");
  const auto secondAliceNode =
      services.references.nameProvider->getNameNode(*model->persons[2]);
  ASSERT_TRUE(secondAliceNode.has_value());

  auto references = services.references.references->findReferences(
      *model->persons[2], {.includeDeclaration = true});
  ASSERT_EQ(references.size(), 2u);
}

TEST(MultiReferenceTest, ReportsUnresolvedReferenceWhenNoCandidateMatches) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  ASSERT_TRUE(register_language(*shared));

  auto document = open_and_build(
      *shared, "unresolved.mr",
      "person Bob\n"
      "hello Alice\n");

  ASSERT_NE(document, nullptr);
  auto *model = dynamic_cast<Model *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);
  ASSERT_EQ(model->greetings.size(), 1u);

  const auto &reference = model->greetings.front()->person;
  EXPECT_TRUE(reference.empty());
  EXPECT_FALSE(reference);
  EXPECT_TRUE(reference.hasError());
  EXPECT_NE(reference.getErrorMessage().find("Alice"), std::string::npos);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.workspace.referenceDescriptionProvider, nullptr);
  EXPECT_TRUE(
      services.workspace.referenceDescriptionProvider->createDescriptions(
          *document)
          .empty());
  EXPECT_TRUE(has_diagnostic_message(*document, "Unresolved reference: Alice"));
}

} // namespace
} // namespace pegium::references
