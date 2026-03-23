#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>

namespace pegium {
namespace {

TEST(ServiceAccessTest, CastsConcreteLanguageServices) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "calc", {".calc"});
  pegium::services::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);

  const auto *raw = services.get();
  ASSERT_NE(as_services(raw), nullptr);
  EXPECT_EQ(as_services(raw)->languageMetaData.languageId, "calc");
}

TEST(ServiceAccessTest, RejectsCoreOnlyServices) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "calc", {".calc"});
  pegium::services::installDefaultCoreServices(*services);

  EXPECT_EQ(as_services(services.get()), nullptr);
}

TEST(ServiceAccessTest, ResolvesServicesFromRegistryHelpers) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_services(*shared, "calc", {".calc"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto uri = test::make_file_uri("feature.calc");
  ASSERT_NE(get_services(*shared->serviceRegistry, uri), nullptr);
}

TEST(ServiceAccessTest, ReturnsNullWhenRegistryCannotResolveUri) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices =
        test::make_uninstalled_services(*shared, "calc", {".calc"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  EXPECT_EQ(get_services(*shared->serviceRegistry,
                         test::make_file_uri("feature.unknown")),
            nullptr);
}

} // namespace
} // namespace pegium
