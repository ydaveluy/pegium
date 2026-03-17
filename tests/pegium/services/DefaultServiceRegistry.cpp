#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>

namespace pegium::services {
namespace {

TEST(DefaultServiceRegistryTest, ResolvesLanguagesByExtensionAndFileName) {
  auto shared = test::make_shared_core_services();

  auto first =
      test::make_core_services(*shared, "calc", {".calc"}, {"Calcfile"});
  auto second = test::make_core_services(*shared, "req", {".req"});

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(first)));
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(second)));

  const auto *calcByExtension =
      shared->serviceRegistry->getServices(test::make_file_uri("main.calc"));
  ASSERT_NE(calcByExtension, nullptr);
  EXPECT_EQ(calcByExtension->languageId, "calc");
  EXPECT_TRUE(
      shared->serviceRegistry->hasServices(test::make_file_uri("main.calc")));

  const auto *calcByFileName =
      shared->serviceRegistry->getServices(test::make_file_uri("Calcfile"));
  ASSERT_NE(calcByFileName, nullptr);
  EXPECT_EQ(calcByFileName->languageId, "calc");

  const auto *requirements =
      shared->serviceRegistry->getServicesByFileName("feature.req");
  ASSERT_NE(requirements, nullptr);
  EXPECT_EQ(requirements->languageId, "req");

  const auto all = shared->serviceRegistry->all();
  ASSERT_EQ(all.size(), 2u);
  ASSERT_NE(all[0], nullptr);
  ASSERT_NE(all[1], nullptr);
  EXPECT_EQ(all[0]->languageId, "calc");
  EXPECT_EQ(all[1]->languageId, "req");
}

TEST(DefaultServiceRegistryTest, RemovesRegisteredLanguage) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "calc", {".calc"})));

  EXPECT_TRUE(shared->serviceRegistry->remove("calc"));
  EXPECT_EQ(shared->serviceRegistry->getServicesByLanguageId("calc"), nullptr);
  EXPECT_FALSE(shared->serviceRegistry->remove("calc"));
}

TEST(DefaultServiceRegistryTest, PrefersOpenedDocumentLanguageIdOverFileExtension) {
  auto shared = test::make_shared_core_services();

  auto first = test::make_core_services(*shared, "calc", {".x"});
  auto second = test::make_core_services(*shared, "req", {".req"});

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(first)));
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(second)));

  const auto uri = test::make_file_uri("live-language.x");
  ASSERT_NE(shared->workspace.textDocuments->open(uri, "req", "content", 1),
            nullptr);

  const auto *services = shared->serviceRegistry->getServices(uri);
  ASSERT_NE(services, nullptr);
  EXPECT_EQ(services->languageId, "req");
}

TEST(DefaultServiceRegistryTest, FileNameHasPriorityOverExtension) {
  auto shared = test::make_shared_core_services();

  auto fileNameServices =
      test::make_core_services(*shared, "named", {}, {"special.calc"});
  auto extensionServices = test::make_core_services(*shared, "calc", {".calc"});

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(fileNameServices)));
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(extensionServices)));

  const auto *byExactFileName = shared->serviceRegistry->getServices(
      test::make_file_uri("special.calc"));
  ASSERT_NE(byExactFileName, nullptr);
  EXPECT_EQ(byExactFileName->languageId, "named");

  const auto *byExtension = shared->serviceRegistry->getServices(
      test::make_file_uri("other.calc"));
  ASSERT_NE(byExtension, nullptr);
  EXPECT_EQ(byExtension->languageId, "calc");
}

TEST(DefaultServiceRegistryTest, ResolutionIsCaseSensitive) {
  auto shared = test::make_shared_core_services();

  auto services =
      test::make_core_services(*shared, "calc", {".calc"}, {"Calcfile"});
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  EXPECT_EQ(shared->serviceRegistry->getServices(
                test::make_file_uri("main.CALC")),
            nullptr);
  EXPECT_EQ(shared->serviceRegistry->getServices(
                test::make_file_uri("calcfile")),
            nullptr);
}

} // namespace
} // namespace pegium::services
