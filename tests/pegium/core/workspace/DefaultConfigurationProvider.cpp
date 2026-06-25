#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/workspace/DefaultConfigurationProvider.hpp>

namespace pegium::workspace {
namespace {

std::future<void> make_ready_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

std::future<std::vector<pegium::JsonValue>>
make_ready_future(std::vector<pegium::JsonValue> value) {
  std::promise<std::vector<pegium::JsonValue>> promise;
  promise.set_value(std::move(value));
  return promise.get_future();
}

TEST(DefaultConfigurationProviderTest,
     InitializedFetchesConfigurationAndPublishesSectionUpdates) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultConfigurationProvider provider(*shared);

  InitializeParams initializeParams{};
  initializeParams.capabilities.workspaceConfiguration = true;
  provider.initialize(initializeParams);

  std::vector<ConfigurationProvider::ConfigurationSectionUpdate> updates;
  auto onUpdate = provider.onConfigurationSectionUpdate(
      [&updates](
          const ConfigurationProvider::ConfigurationSectionUpdate &update) {
        updates.push_back(update);
      });
  (void)onUpdate;

  std::vector<std::string> registeredSections;
  std::vector<std::string> fetchedSections;
  InitializedParams initializedParams{};
  initializedParams.registerDidChangeConfiguration =
      [&registeredSections](std::vector<std::string> sections) {
        registeredSections = std::move(sections);
        return make_ready_future();
      };
  initializedParams.fetchConfiguration =
      [&fetchedSections](std::vector<std::string> sections) {
        fetchedSections = std::move(sections);
        pegium::JsonValue::Object buildConfig;
        buildConfig.try_emplace("workspaceRoot", "src");

        pegium::JsonValue::Object pegiumConfig;
        pegiumConfig.try_emplace("build", std::move(buildConfig));

        pegium::JsonValue::Object validationConfig;
        validationConfig.try_emplace("enabled", false);
        validationConfig.try_emplace(
            "categories",
            pegium::JsonValue::Array{pegium::JsonValue("syntax"),
                                       pegium::JsonValue("types")});

        pegium::JsonValue::Object sectionConfig;
        sectionConfig.try_emplace("validation", std::move(validationConfig));
        return make_ready_future(std::vector<pegium::JsonValue>{
            pegium::JsonValue(pegiumConfig),
            pegium::JsonValue(sectionConfig)});
      };

  auto future = provider.initialized(initializedParams);
  future.get();

  ASSERT_TRUE(provider.isReady());
  EXPECT_EQ(registeredSections, (std::vector<std::string>{"pegium", "test"}));
  EXPECT_EQ(fetchedSections, (std::vector<std::string>{"pegium", "test"}));
  ASSERT_EQ(updates.size(), 2u);
  EXPECT_EQ(updates[0].section, "pegium");
  EXPECT_EQ(updates[1].section, "test");

  const auto buildValue = provider.getConfiguration("pegium", "build");
  ASSERT_TRUE(buildValue.has_value());
  ASSERT_TRUE(buildValue->isObject());
  EXPECT_EQ(buildValue->object().at("workspaceRoot").string(), "src");

  const auto validationValue = provider.getConfiguration("test", "validation");
  ASSERT_TRUE(validationValue.has_value());
  ASSERT_TRUE(validationValue->isObject());

  const auto configuration = provider.getWorkspaceConfiguration(
      test::make_file_uri("configuration.test"));
  ASSERT_TRUE(std::holds_alternative<bool>(configuration.validation));
  EXPECT_FALSE(std::get<bool>(configuration.validation));
}

// Regression: when the provider is shared_ptr-managed (the production case),
// the async initialized() task writes back into the provider's members after a
// slow client round-trip. If SharedServices is torn down meanwhile, a task that
// captured a raw `this` would use freed memory. The task must keep the provider
// alive for its whole duration.
TEST(DefaultConfigurationProviderTest,
     InitializedKeepsSharedOwnedProviderAliveUntilAsyncCompletes) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);

  auto provider = std::make_shared<DefaultConfigurationProvider>(*shared);
  const std::weak_ptr<DefaultConfigurationProvider> weak = provider;

  InitializeParams initializeParams{};
  initializeParams.capabilities.workspaceConfiguration = true;
  provider->initialize(initializeParams);

  std::promise<void> fetchEntered;
  std::promise<void> releaseFetch;
  auto fetchEnteredFuture = fetchEntered.get_future();
  auto releaseFetchFuture = releaseFetch.get_future();

  InitializedParams initializedParams{};
  initializedParams.fetchConfiguration =
      [&](std::vector<std::string>) {
        return std::async(std::launch::async,
                          [&]() -> std::vector<pegium::JsonValue> {
                            fetchEntered.set_value();
                            releaseFetchFuture.wait();
                            return {};
                          });
      };

  auto future = provider->initialized(initializedParams);

  // The async task is now blocked inside fetchConfiguration.
  fetchEnteredFuture.wait();

  // Drop the only owning reference, mirroring SharedServices teardown while the
  // task is still in flight. The running task must keep the provider alive.
  provider.reset();
  EXPECT_FALSE(weak.expired());

  // Let the task finish, then release the async shared state (which holds the
  // task's strong reference). Only then may the provider be destroyed.
  releaseFetch.set_value();
  future.get();
  future = std::future<void>{};
  EXPECT_TRUE(weak.expired());
}

TEST(DefaultConfigurationProviderTest,
     MissingConfigurationValueReturnsNullopt) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultConfigurationProvider provider(*shared);
  EXPECT_FALSE(provider.getConfiguration("missing", "prop").has_value());
}

TEST(DefaultConfigurationProviderTest, UpdateConfigurationStoresAndPublishes) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultConfigurationProvider provider(*shared);

  std::vector<ConfigurationProvider::ConfigurationSectionUpdate> updates;
  auto onUpdate = provider.onConfigurationSectionUpdate(
      [&updates](const auto &update) { updates.push_back(update); });
  (void)onUpdate;

  pegium::JsonValue::Object settings;
  settings.try_emplace("prop", "foo");

  ConfigurationChangeParams params;
  params.settings = pegium::JsonValue(
      pegium::JsonValue::Object{{"someLang", pegium::JsonValue(settings)}});

  provider.updateConfiguration(params);

  const auto value = provider.getConfiguration("someLang", "prop");
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->isString());
  EXPECT_EQ(value->string(), "foo");
  ASSERT_EQ(updates.size(), 1u);
  EXPECT_EQ(updates.front().section, "someLang");
}

TEST(DefaultConfigurationProviderTest,
     UpdateConfigurationOverwritesLanguageSettings) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultConfigurationProvider provider(*shared);

  ConfigurationChangeParams params;
  params.settings = pegium::JsonValue(pegium::JsonValue::Object{
      {"someLang",
       pegium::JsonValue(pegium::JsonValue::Object{{"prop", "bar"}})}});
  provider.updateConfiguration(params);

  params.settings = pegium::JsonValue(pegium::JsonValue::Object{
      {"someLang",
       pegium::JsonValue(pegium::JsonValue::Object{{"prop", "bar2"}})}});
  provider.updateConfiguration(params);

  const auto value = provider.getConfiguration("someLang", "prop");
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->isString());
  EXPECT_EQ(value->string(), "bar2");
}

} // namespace
} // namespace pegium::workspace
