#include <gtest/gtest.h>

#include <lsp/connection.h>

#include <memory>
#include <type_traits>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {
namespace {

struct ExtendedSharedServices final : SharedServices {
  struct {
    std::shared_ptr<int> telemetry;
  } observability;

  explicit ExtendedSharedServices(bool *destroyed = nullptr)
      : destroyed(destroyed) {}

  ~ExtendedSharedServices() override {
    if (destroyed != nullptr) {
      *destroyed = true;
    }
  }

private:
  bool *destroyed = nullptr;
};

struct ExtendedCoreServices final : services::CoreServices {
  struct {
    std::shared_ptr<int> telemetry;
  } observability;

  explicit ExtendedCoreServices(const services::SharedCoreServices &sharedServices,
                                bool *destroyed = nullptr)
      : services::CoreServices(sharedServices), destroyed(destroyed) {}

  ~ExtendedCoreServices() noexcept override {
    if (destroyed != nullptr) {
      *destroyed = true;
    }
  }

private:
  bool *destroyed = nullptr;
};

struct ExtendedServices final : Services {
  struct {
    std::shared_ptr<int> telemetry;
  } observability;

  explicit ExtendedServices(const SharedServices &sharedServices,
                            bool *destroyed = nullptr)
      : Services(sharedServices), destroyed(destroyed) {}

  ~ExtendedServices() noexcept override {
    if (destroyed != nullptr) {
      *destroyed = true;
    }
  }

private:
  bool *destroyed = nullptr;
};

static_assert(std::has_virtual_destructor_v<services::SharedCoreServices>);
static_assert(std::has_virtual_destructor_v<SharedServices>);
static_assert(std::has_virtual_destructor_v<services::CoreServices>);
static_assert(std::has_virtual_destructor_v<Services>);

TEST(SharedServicesTest, CanBeExtendedWithCustomSections) {
  ExtendedSharedServices shared;

  services::installDefaultSharedCoreServices(shared);
  installDefaultSharedLspServices(shared);

  shared.observability.telemetry = std::make_shared<int>(42);

  ASSERT_NE(shared.observability.telemetry, nullptr);
  EXPECT_EQ(*shared.observability.telemetry, 42);
  ASSERT_NE(shared.lsp.textDocuments, nullptr);
  ASSERT_NE(shared.workspace.textDocuments, nullptr);
  EXPECT_EQ(shared.lsp.textDocuments.get(), shared.workspace.textDocuments.get());
}

TEST(SharedServicesTest, InitializeSharedServicesForLanguageServerBootstrapsLspReadyShared) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);

  SharedServices shared;
  initializeSharedServicesForLanguageServer(shared, connection);

  ASSERT_NE(shared.astReflection, nullptr);
  ASSERT_NE(shared.serviceRegistry, nullptr);
  ASSERT_NE(shared.workspace.documents, nullptr);
  ASSERT_NE(shared.workspace.documentBuilder, nullptr);
  ASSERT_NE(shared.workspace.workspaceManager, nullptr);
  ASSERT_NE(shared.lsp.textDocuments, nullptr);
  ASSERT_NE(shared.lsp.languageServer, nullptr);
  ASSERT_NE(shared.lsp.documentUpdateHandler, nullptr);
  ASSERT_NE(shared.workspace.textDocuments, nullptr);
  EXPECT_EQ(shared.lsp.languageClient.get(), nullptr);
  EXPECT_EQ(shared.lsp.textDocuments.get(), shared.workspace.textDocuments.get());
}

TEST(SharedServicesTest, CoreServicesCanBeExtendedWithCustomSections) {
  ExtendedSharedServices shared;
  services::installDefaultSharedCoreServices(shared);

  ExtendedCoreServices languageServices{shared};
  services::installDefaultCoreServices(languageServices);

  languageServices.observability.telemetry = std::make_shared<int>(7);

  ASSERT_NE(languageServices.observability.telemetry, nullptr);
  EXPECT_EQ(*languageServices.observability.telemetry, 7);
  EXPECT_NE(languageServices.references.references, nullptr);
  EXPECT_NE(languageServices.workspace.astNodeLocator, nullptr);
}

TEST(SharedServicesTest, LanguageServicesCanBeExtendedWithCustomSections) {
  ExtendedSharedServices shared;
  services::installDefaultSharedCoreServices(shared);
  installDefaultSharedLspServices(shared);

  ExtendedServices languageServices{shared};
  services::installDefaultCoreServices(languageServices);
  installDefaultLspServices(languageServices);

  languageServices.observability.telemetry = std::make_shared<int>(99);

  ASSERT_NE(languageServices.observability.telemetry, nullptr);
  EXPECT_EQ(*languageServices.observability.telemetry, 99);
  EXPECT_NE(languageServices.lsp.definitionProvider, nullptr);
  EXPECT_NE(languageServices.lsp.referencesProvider, nullptr);
}

TEST(SharedServicesTest, MakeDefaultServicesBootstrapsBaselineLanguageServices) {
  SharedServices shared;
  services::installDefaultSharedCoreServices(shared);
  installDefaultSharedLspServices(shared);

  auto services =
      services::makeDefaultServices(shared, "default-language");

  ASSERT_NE(services, nullptr);
  EXPECT_EQ(services->languageMetaData.languageId, "default-language");
  EXPECT_NE(services->references.nameProvider, nullptr);
  EXPECT_NE(services->references.scopeProvider, nullptr);
  EXPECT_NE(services->validation.validationRegistry, nullptr);
  EXPECT_NE(services->lsp.completionProvider, nullptr);
  EXPECT_NE(services->lsp.definitionProvider, nullptr);
  EXPECT_NE(services->lsp.renameProvider, nullptr);
  EXPECT_EQ(services->parser, nullptr);
  EXPECT_EQ(services->lsp.formatter, nullptr);
}

TEST(SharedServicesTest, MakeDefaultServicesSupportsDerivedLanguageServices) {
  ExtendedSharedServices shared;
  services::installDefaultSharedCoreServices(shared);
  installDefaultSharedLspServices(shared);

  auto services =
      services::makeDefaultServices<ExtendedServices>(shared, "extended-language");

  ASSERT_NE(services, nullptr);
  EXPECT_EQ(services->languageMetaData.languageId, "extended-language");
  EXPECT_NE(services->references.references, nullptr);
  EXPECT_NE(services->workspace.astNodeLocator, nullptr);
  EXPECT_NE(services->lsp.documentSymbolProvider, nullptr);
  EXPECT_NE(services->lsp.referencesProvider, nullptr);
}

TEST(SharedServicesTest, SupportsDestructionThroughBasePointers) {
  bool destroyed = false;

  {
    auto derived = std::make_unique<ExtendedSharedServices>(&destroyed);
    services::installDefaultSharedCoreServices(*derived);
    installDefaultSharedLspServices(*derived);

    std::unique_ptr<services::SharedCoreServices> base = std::move(derived);
    ASSERT_NE(base, nullptr);
  }

  EXPECT_TRUE(destroyed);
}

TEST(SharedServicesTest, SupportsCoreAndLanguageDestructionThroughBasePointers) {
  ExtendedSharedServices shared;
  services::installDefaultSharedCoreServices(shared);
  installDefaultSharedLspServices(shared);

  bool coreDestroyed = false;
  bool languageDestroyed = false;

  {
    auto derived = std::make_unique<ExtendedCoreServices>(shared, &coreDestroyed);
    services::installDefaultCoreServices(*derived);
    std::unique_ptr<services::CoreServices> base = std::move(derived);
    ASSERT_NE(base, nullptr);
  }

  {
    auto derived = std::make_unique<ExtendedServices>(shared, &languageDestroyed);
    services::installDefaultCoreServices(*derived);
    installDefaultLspServices(*derived);
    std::unique_ptr<services::CoreServices> base = std::move(derived);
    ASSERT_NE(base, nullptr);
  }

  EXPECT_TRUE(coreDestroyed);
  EXPECT_TRUE(languageDestroyed);
}

} // namespace
} // namespace pegium
