#include <gtest/gtest.h>

#include <ranges>
#include <string>
#include <stdexcept>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

namespace pegium::services {
namespace {

using namespace pegium::parser;

struct ReflectionBaseNode : AstNode {};
struct ReflectionDerivedNode final : ReflectionBaseNode {};
struct ReflectionRootNode : AstNode {
  vector<pointer<ReflectionBaseNode>> nodes;
};

struct ReferenceTargetBase : AstNode {};
struct ReferenceTargetDerived final : ReferenceTargetBase {
  std::string name;
};
struct ReferenceBootstrapRoot : AstNode {
  vector<pointer<AstNode>> declarations;
  reference<ReferenceTargetBase> target;
};

class ReflectionBootstrapParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  Rule<ReflectionDerivedNode> DerivedRule{"Derived", "item"_kw};
  Rule<ReflectionBaseNode> BaseRule{"Base", DerivedRule};
  Rule<ReflectionRootNode> RootRule{
      "Root", some(append<&ReflectionRootNode::nodes>(BaseRule))};
};

class ReferenceBootstrapParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<ReferenceTargetDerived> Declaration{
      "Declaration", "def"_kw + assign<&ReferenceTargetDerived::name>(ID) + ";"_kw};
  Rule<ReferenceBootstrapRoot> RootRule{
      "Root",
      many(append<&ReferenceBootstrapRoot::declarations>(Declaration)) +
          "use"_kw + assign<&ReferenceBootstrapRoot::target>(ID) + ";"_kw};
};

TEST(DefaultServiceRegistryTest, ResolvesLanguagesByExtensionAndFileName) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto first =
      test::make_uninstalled_core_services(*shared, "calc", {".calc"}, {"Calcfile"});
  pegium::services::installDefaultCoreServices(*first);
  auto second = test::make_uninstalled_core_services(*shared, "req", {".req"});
  pegium::services::installDefaultCoreServices(*second);

  shared->serviceRegistry->registerServices(std::move(first));
  shared->serviceRegistry->registerServices(std::move(second));

  const auto &calcByExtension =
      shared->serviceRegistry->getServices(test::make_file_uri("main.calc"));
  EXPECT_EQ(calcByExtension.languageMetaData.languageId, "calc");
  EXPECT_EQ(shared->serviceRegistry->findServices(test::make_file_uri("main.calc")),
            &calcByExtension);

  const auto &calcByFileName =
      shared->serviceRegistry->getServices(test::make_file_uri("Calcfile"));
  EXPECT_EQ(calcByFileName.languageMetaData.languageId, "calc");

  const auto &requirements =
      shared->serviceRegistry->getServices(test::make_file_uri("feature.req"));
  EXPECT_EQ(requirements.languageMetaData.languageId, "req");

  const auto all = shared->serviceRegistry->all();
  ASSERT_EQ(all.size(), 2u);
  ASSERT_NE(all[0], nullptr);
  ASSERT_NE(all[1], nullptr);
  EXPECT_EQ(all[0]->languageMetaData.languageId, "calc");
  EXPECT_EQ(all[1]->languageMetaData.languageId, "req");
}

TEST(DefaultServiceRegistryTest, ThrowsWhenRegistryIsEmptyOrUriIsUnknown) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  EXPECT_THROW(
      (void)shared->serviceRegistry->getServices(test::make_file_uri("main.calc")),
      std::runtime_error);

  {

    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "calc", {".calc"});

    pegium::services::installDefaultCoreServices(*registeredServices);

    shared->serviceRegistry->registerServices(std::move(registeredServices));

  }
  EXPECT_THROW(
      (void)shared->serviceRegistry->getServices(test::make_file_uri("main.req")),
      std::runtime_error);
  EXPECT_EQ(shared->serviceRegistry->findServices(test::make_file_uri("main.req")),
            nullptr);
}

TEST(DefaultServiceRegistryTest, PrefersOpenedDocumentLanguageIdOverFileExtension) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto first = test::make_uninstalled_core_services(*shared, "calc", {".x"});
  pegium::services::installDefaultCoreServices(*first);
  auto second = test::make_uninstalled_core_services(*shared, "req", {".req"});
  pegium::services::installDefaultCoreServices(*second);

  shared->serviceRegistry->registerServices(std::move(first));
  shared->serviceRegistry->registerServices(std::move(second));

  const auto uri = test::make_file_uri("live-language.x");
  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  ASSERT_NE(test::set_text_document(*documents, uri, "req", "content", 1),
            nullptr);

  const auto &services = shared->serviceRegistry->getServices(uri);
  EXPECT_EQ(services.languageMetaData.languageId, "req");
}

TEST(DefaultServiceRegistryTest, FileNameHasPriorityOverExtension) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto fileNameServices =
      test::make_uninstalled_core_services(*shared, "named", {}, {"special.calc"});
  pegium::services::installDefaultCoreServices(*fileNameServices);
  auto extensionServices = test::make_uninstalled_core_services(*shared, "calc", {".calc"});
  pegium::services::installDefaultCoreServices(*extensionServices);

  shared->serviceRegistry->registerServices(std::move(fileNameServices));
  shared->serviceRegistry->registerServices(std::move(extensionServices));

  const auto &byExactFileName = shared->serviceRegistry->getServices(
      test::make_file_uri("special.calc"));
  EXPECT_EQ(byExactFileName.languageMetaData.languageId, "named");

  const auto &byExtension = shared->serviceRegistry->getServices(
      test::make_file_uri("other.calc"));
  EXPECT_EQ(byExtension.languageMetaData.languageId, "calc");
}

TEST(DefaultServiceRegistryTest, ResolutionIsCaseSensitive) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto services =
      test::make_uninstalled_core_services(*shared, "calc", {".calc"}, {"Calcfile"});
  pegium::services::installDefaultCoreServices(*services);
  shared->serviceRegistry->registerServices(std::move(services));

  EXPECT_THROW(
      (void)shared->serviceRegistry->getServices(test::make_file_uri("main.CALC")),
      std::runtime_error);
  EXPECT_THROW(
      (void)shared->serviceRegistry->getServices(test::make_file_uri("calcfile")),
      std::runtime_error);
}

TEST(DefaultServiceRegistryTest,
     LastRegistrationWinsForCollidingExtensionsAndFileNames) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto recordingSink = std::make_shared<test::RecordingObservabilitySink>();
  shared->observabilitySink = recordingSink;
  auto first =
      test::make_uninstalled_core_services(*shared, "first", {".calc"}, {"Calcfile"});
  pegium::services::installDefaultCoreServices(*first);
  auto second =
      test::make_uninstalled_core_services(*shared, "second", {".calc"}, {"Calcfile"});
	  pegium::services::installDefaultCoreServices(*second);

  shared->serviceRegistry->registerServices(std::move(first));
  shared->serviceRegistry->registerServices(std::move(second));
  ASSERT_TRUE(recordingSink->waitForCount(2));
  const auto warnings = recordingSink->observations();

  ASSERT_EQ(warnings.size(), 2u);
  EXPECT_EQ(warnings[0].code, observability::ObservationCode::LanguageMappingCollision);
  EXPECT_EQ(warnings[0].languageId, "second");
  EXPECT_NE(warnings[0].message.find("file extension .calc"), std::string::npos);
  EXPECT_EQ(warnings[1].code, observability::ObservationCode::LanguageMappingCollision);
  EXPECT_EQ(warnings[1].languageId, "second");
  EXPECT_NE(warnings[1].message.find("file name Calcfile"), std::string::npos);
  EXPECT_EQ(shared->serviceRegistry
                ->getServices(test::make_file_uri("sample.calc"))
                .languageMetaData.languageId,
            "second");
  EXPECT_EQ(shared->serviceRegistry
                ->getServices(test::make_file_uri("Calcfile"))
                .languageMetaData.languageId,
            "second");
}

TEST(DefaultServiceRegistryTest,
     ReplacingRegisteredLanguageKeepsOrderAndRefreshesMappings) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "calc", {".calc"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "req", {".req"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "calc", {".calc2"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto all = shared->serviceRegistry->all();
  ASSERT_EQ(all.size(), 2u);
  EXPECT_EQ(all[0]->languageMetaData.languageId, "calc");
  EXPECT_EQ(all[1]->languageMetaData.languageId, "req");
  EXPECT_THROW(
      (void)shared->serviceRegistry->getServices(test::make_file_uri("main.calc")),
      std::runtime_error);
  EXPECT_EQ(shared->serviceRegistry
                ->getServices(test::make_file_uri("main.calc2"))
                .languageMetaData.languageId,
            "calc");
}

TEST(DefaultServiceRegistryTest, InstallsAstNodeLocatorByDefault) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "calc", {".calc"});
  pegium::services::installDefaultCoreServices(*services);

  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->workspace.astNodeLocator, nullptr);
  EXPECT_TRUE(services->isComplete());
}

TEST(DefaultServiceRegistryTest, RequiresAstNodeLocatorForRegistration) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "calc", {".calc"});
  pegium::services::installDefaultCoreServices(*services);

  ASSERT_NE(services, nullptr);
  services->workspace.astNodeLocator.reset();

  EXPECT_FALSE(services->isComplete());
  EXPECT_THROW(shared->serviceRegistry->registerServices(std::move(services)),
               std::invalid_argument);
}

TEST(DefaultServiceRegistryTest, BootstrapsAstReflectionFromParserGrammar) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services<ReflectionBootstrapParser>(
      *shared, "reflection", {".reflection"});
  pegium::services::installDefaultCoreServices(*services);

  shared->serviceRegistry->registerServices(std::move(services));

  ASSERT_NE(shared->astReflection, nullptr);
  EXPECT_TRUE(shared->astReflection->isSubtype(
      std::type_index(typeid(ReflectionDerivedNode)),
      std::type_index(typeid(ReflectionBaseNode))));
  const auto &knownTypes = shared->astReflection->getAllTypes();
  EXPECT_NE(std::ranges::find(knownTypes, std::type_index(typeid(ReflectionRootNode))),
            knownTypes.end());
  EXPECT_NE(std::ranges::find(knownTypes, std::type_index(typeid(ReflectionBaseNode))),
            knownTypes.end());
}

TEST(DefaultServiceRegistryTest,
     BootstrapsReferenceTargetSubtypesFromParserGrammar) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services<ReferenceBootstrapParser>(
      *shared, "reference-bootstrap", {".rb"});
  pegium::services::installDefaultCoreServices(*services);

  shared->serviceRegistry->registerServices(std::move(services));

  ASSERT_NE(shared->astReflection, nullptr);
  EXPECT_TRUE(shared->astReflection->isSubtype(
      std::type_index(typeid(ReferenceTargetDerived)),
      std::type_index(typeid(ReferenceTargetBase))));
}

} // namespace
} // namespace pegium::services
