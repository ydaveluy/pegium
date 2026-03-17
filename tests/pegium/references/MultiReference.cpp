#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/workspace/AstNodeLocator.hpp>

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
  return sharedServices.serviceRegistry->registerServices(
      test::make_core_services<MultiReferenceParser>(sharedServices,
                                                     "multi-ref", {".mr"}));
}

std::shared_ptr<workspace::Document>
open_and_build(services::SharedCoreServices &sharedServices,
               std::string_view fileName, std::string text) {
  return test::open_and_build_document(sharedServices, test::make_file_uri(fileName),
                                       "multi-ref", std::move(text));
}

TEST(MultiReferenceTest, ResolvesAllMatchingDeclarationsInSameDocument) {
  auto shared = test::make_shared_core_services();
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

  const auto &reference = model->greetings.front()->person;
  EXPECT_EQ(reference.getRefText(), "Alice");
  EXPECT_FALSE(reference.hasError());
  ASSERT_EQ(reference.items().size(), 2u);

  std::unordered_set<std::string> targetPaths;
  for (const auto &item : reference.items()) {
    ASSERT_NE(item.ref, nullptr);
    EXPECT_EQ(item.ref->name, "Alice");
    ASSERT_NE(item.description, nullptr);
    EXPECT_EQ(item.description->documentId, document->id);
    targetPaths.insert(workspace::AstNodeLocator::getAstNodePath(*item.ref));
  }
  EXPECT_EQ(targetPaths.size(), 2u);

  ASSERT_EQ(document->referenceDescriptions.size(), 2u);
  for (const auto &description : document->referenceDescriptions) {
    EXPECT_EQ(description.sourceText(document->textView()), "Alice");
    EXPECT_TRUE(description.isResolved());
    ASSERT_TRUE(description.targetDocumentId.has_value());
    EXPECT_EQ(*description.targetDocumentId, document->id);
  }
}

TEST(MultiReferenceTest, ResolvesAllMatchingDeclarationsAcrossDocuments) {
  auto shared = test::make_shared_core_services();
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
  ASSERT_EQ(reference.items().size(), 3u);

  std::unordered_map<workspace::DocumentId, std::size_t> targetsByDocumentId;
  for (const auto &item : reference.items()) {
    ASSERT_NE(item.description, nullptr);
    ++targetsByDocumentId[item.description->documentId];
  }

  EXPECT_EQ(targetsByDocumentId[firstDocument->id], 1u);
  EXPECT_EQ(targetsByDocumentId[secondDocument->id], 2u);
}

TEST(MultiReferenceTest, IndexesOneLogicalReferenceAgainstEachResolvedTarget) {
  auto shared = test::make_shared_core_services();
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
  ASSERT_EQ(document->referenceDescriptions.size(), 2u);

  const auto firstAliceKey = workspace::NodeKey{
      .documentId = document->id,
      .symbolId = document->makeSymbolId(*model->persons[0])};
  const auto secondAliceKey = workspace::NodeKey{
      .documentId = document->id,
      .symbolId = document->makeSymbolId(*model->persons[2])};

  auto firstReferences = utils::collect(
      shared->workspace.indexManager->findAllReferences(firstAliceKey, true));
  auto secondReferences = utils::collect(
      shared->workspace.indexManager->findAllReferences(secondAliceKey, true));

  ASSERT_EQ(firstReferences.size(), 2u);
  ASSERT_EQ(secondReferences.size(), 2u);
}

TEST(MultiReferenceTest,
     ReferencesServiceIncludesSharedUsageForEachMatchingDeclaration) {
  auto shared = test::make_shared_core_services();
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

  const auto *services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->references.references, nullptr);
  ASSERT_NE(services->references.nameProvider, nullptr);

  const auto secondAliceName =
      services->references.nameProvider->getNameNode(*model->persons[2]);
  ASSERT_TRUE(secondAliceName.valid());

  auto references = utils::collect(
      services->references.references->findReferencesAt(
          *document, secondAliceName.getBegin(), true));
  ASSERT_EQ(references.size(), 2u);
}

TEST(MultiReferenceTest, ReportsUnresolvedReferenceWhenNoCandidateMatches) {
  auto shared = test::make_shared_core_services();
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
  EXPECT_TRUE(reference.items().empty());
  EXPECT_TRUE(reference.hasError());
  EXPECT_NE(reference.getErrorMessage().find("Alice"), std::string::npos);

  EXPECT_TRUE(document->referenceDescriptions.empty());
  EXPECT_TRUE(has_diagnostic_message(*document, "Unresolved reference: Alice"));
}

} // namespace
} // namespace pegium::references
