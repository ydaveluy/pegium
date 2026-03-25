#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <typeindex>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

namespace pegium::integration {
namespace {

using namespace pegium::parser;

bool has_diagnostic_message(const workspace::Document &document,
                            std::string_view needle) {
  for (const auto &diagnostic : document.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

struct CrossLanguageAbstractElement : AstNode {
  [[nodiscard]] virtual std::string_view kind() const noexcept = 0;
};

struct CrossLanguageProvidedElement final : CrossLanguageAbstractElement {
  string name;

  [[nodiscard]] std::string_view kind() const noexcept override {
    return "provided";
  }
};

struct CrossLanguageProviderRoot final : AstNode {
  vector<pointer<CrossLanguageAbstractElement>> elements;
};

struct CrossLanguageConsumerRoot final : AstNode {
  reference<CrossLanguageAbstractElement> target;
};

class CrossLanguageProviderParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  [[nodiscard]] const grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<CrossLanguageProvidedElement> ElementRule{
      "Element", "def"_kw + assign<&CrossLanguageProvidedElement::name>(ID) + ";"_kw};
  Rule<CrossLanguageProviderRoot> RootRule{
      "Root", some(append<&CrossLanguageProviderRoot::elements>(ElementRule))};
};

class CrossLanguageConsumerParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  [[nodiscard]] const grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<CrossLanguageConsumerRoot> RootRule{
      "Root", "use"_kw + assign<&CrossLanguageConsumerRoot::target>(ID) + ";"_kw};
};

enum class RegistrationOrder {
  ConsumerThenProvider,
  ProviderThenConsumer,
};

class CrossLanguageReferencesIntegrationTest
    : public ::testing::TestWithParam<RegistrationOrder> {
protected:
  std::unique_ptr<pegium::SharedServices> shared =
      pegium::test::make_empty_shared_services();

  CrossLanguageReferencesIntegrationTest() {
    pegium::installDefaultSharedCoreServices(*shared);
    pegium::installDefaultSharedLspServices(*shared);
    pegium::test::initialize_shared_workspace_for_tests(*shared);
  }

  void registerLanguages() {
    auto consumer = test::make_uninstalled_services<CrossLanguageConsumerParser>(
        *shared, "cross-consumer", {".consumer"});
    pegium::installDefaultCoreServices(*consumer);

    auto provider = test::make_uninstalled_services<CrossLanguageProviderParser>(
        *shared, "cross-provider", {".provider"});
    pegium::installDefaultCoreServices(*provider);

    if (GetParam() == RegistrationOrder::ConsumerThenProvider) {
      shared->serviceRegistry->registerServices(std::move(consumer));
      shared->serviceRegistry->registerServices(std::move(provider));
      return;
    }

    shared->serviceRegistry->registerServices(std::move(provider));
    shared->serviceRegistry->registerServices(std::move(consumer));
  }

  [[nodiscard]] std::shared_ptr<workspace::Document>
  openValidatedDocument(std::string_view fileName, std::string languageId,
                        std::string text) {
    auto document = test::open_and_build_document(
        *shared, test::make_file_uri(fileName), std::move(languageId),
        std::move(text));
    if (document != nullptr) {
      (void)shared->workspace.documentBuilder->waitUntil(
          workspace::DocumentState::Validated, document->id);
      document = shared->workspace.documents->getDocument(document->id);
    }
    return document;
  }
};

TEST_P(CrossLanguageReferencesIntegrationTest,
       ResolvesReferenceToSubtypeDefinedInAnotherLanguage) {
  registerLanguages();

  ASSERT_NE(shared->astReflection, nullptr);
  EXPECT_TRUE(shared->astReflection->isSubtype(
      std::type_index(typeid(CrossLanguageProvidedElement)),
      std::type_index(typeid(CrossLanguageAbstractElement))));

  auto providerDocument = openValidatedDocument(
      "cross-language.provider", "cross-provider", "def Alpha;");
  ASSERT_NE(providerDocument, nullptr);
  ASSERT_TRUE(providerDocument->parseSucceeded());
  EXPECT_EQ(providerDocument->state, workspace::DocumentState::Validated);

  auto consumerDocument = openValidatedDocument(
      "cross-language.consumer", "cross-consumer", "use Alpha;");
  ASSERT_NE(consumerDocument, nullptr);
  ASSERT_TRUE(consumerDocument->parseSucceeded());
  EXPECT_EQ(consumerDocument->state, workspace::DocumentState::Validated);
  EXPECT_FALSE(
      has_diagnostic_message(*consumerDocument, "Unresolved reference"));

  auto *consumer = dynamic_cast<CrossLanguageConsumerRoot *>(
      consumerDocument->parseResult.value.get());
  ASSERT_NE(consumer, nullptr);

  const auto *target = consumer->target.get();
  ASSERT_NE(target, nullptr);
  EXPECT_FALSE(consumer->target.hasError());
  EXPECT_TRUE(consumer->target.getErrorMessage().empty());

  const auto *provided =
      dynamic_cast<const CrossLanguageProvidedElement *>(target);
  ASSERT_NE(provided, nullptr);
  EXPECT_EQ(provided->name, "Alpha");
  EXPECT_EQ(provided->kind(), "provided");
}

INSTANTIATE_TEST_SUITE_P(
    RegistrationOrders, CrossLanguageReferencesIntegrationTest,
    ::testing::Values(RegistrationOrder::ConsumerThenProvider,
                      RegistrationOrder::ProviderThenConsumer));

} // namespace
} // namespace pegium::integration
