#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/ServiceAccess.hpp>

namespace pegium::lsp {
namespace {

TEST(ServiceAccessTest, CastsConcreteLanguageServices) {
  auto shared = test::make_shared_services();
  auto services = test::make_services(*shared, "calc", {".calc"});

  const auto *raw = services.get();
  ASSERT_NE(as_services(raw), nullptr);
  EXPECT_EQ(as_services(raw)->languageId, "calc");
}

TEST(ServiceAccessTest, RejectsCoreOnlyServices) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "calc", {".calc"});

  EXPECT_EQ(as_services(services.get()), nullptr);
}

TEST(ServiceAccessTest, ResolvesServicesFromRegistryHelpers) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "calc", {".calc"})));

  const auto uri = test::make_file_uri("feature.calc");
  ASSERT_NE(get_services(shared->serviceRegistry.get(), "calc"), nullptr);
  ASSERT_NE(get_services_for_uri(shared->serviceRegistry.get(), uri), nullptr);
  ASSERT_NE(get_services_for_file_name(shared->serviceRegistry.get(),
                                       "feature.calc"),
            nullptr);
}

TEST(ServiceAccessTest, HandlesNullRegistry) {
  EXPECT_EQ(get_services(nullptr, "calc"), nullptr);
  EXPECT_EQ(get_services_for_uri(nullptr, test::make_file_uri("feature.calc")),
            nullptr);
  EXPECT_EQ(get_services_for_file_name(nullptr, "feature.calc"), nullptr);
}

} // namespace
} // namespace pegium::lsp
