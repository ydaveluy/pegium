#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/workspace/DefaultConfigurationProvider.hpp>

namespace pegium::workspace {
namespace {

std::future<void> make_ready_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

std::future<std::vector<services::JsonValue>>
make_ready_future(std::vector<services::JsonValue> value) {
  std::promise<std::vector<services::JsonValue>> promise;
  promise.set_value(std::move(value));
  return promise.get_future();
}

TEST(DefaultConfigurationProviderTest,
     InitializedFetchesConfigurationAndPublishesSectionUpdates) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  DefaultConfigurationProvider provider(shared->serviceRegistry.get());

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
        services::JsonValue::Object buildConfig;
        buildConfig.emplace("ignorePatterns", "generated, out");

        services::JsonValue::Object pegiumConfig;
        pegiumConfig.emplace("build", std::move(buildConfig));

        services::JsonValue::Object validationConfig;
        validationConfig.emplace("enabled", false);
        validationConfig.emplace(
            "categories",
            services::JsonValue::Array{services::JsonValue("syntax"),
                                       services::JsonValue("types")});

        services::JsonValue::Object sectionConfig;
        sectionConfig.emplace("validation", std::move(validationConfig));
        return make_ready_future(
            std::vector<services::JsonValue>{services::JsonValue(pegiumConfig),
                                             services::JsonValue(sectionConfig)});
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
  EXPECT_EQ(buildValue->object().at("ignorePatterns").string(), "generated, out");

  const auto validationValue =
      provider.getConfiguration("test", "validation");
  ASSERT_TRUE(validationValue.has_value());
  ASSERT_TRUE(validationValue->isObject());

  const auto configuration =
      provider.getWorkspaceConfiguration(
          test::make_file_uri("configuration.test"));
  EXPECT_FALSE(configuration.validation.enabled);
  ASSERT_EQ(configuration.validation.categories.size(), 2u);
  EXPECT_EQ(configuration.validation.categories[0], "syntax");
  EXPECT_EQ(configuration.validation.categories[1], "types");
}

TEST(DefaultConfigurationProviderTest, MissingConfigurationValueReturnsNullopt) {
  DefaultConfigurationProvider provider(nullptr);
  EXPECT_FALSE(provider.getConfiguration("missing", "prop").has_value());
}

TEST(DefaultConfigurationProviderTest, UpdateConfigurationStoresAndPublishes) {
  DefaultConfigurationProvider provider(nullptr);

  std::vector<ConfigurationProvider::ConfigurationSectionUpdate> updates;
  auto onUpdate = provider.onConfigurationSectionUpdate(
      [&updates](const auto &update) { updates.push_back(update); });
  (void)onUpdate;

  services::JsonValue::Object settings;
  settings.emplace("prop", "foo");

  ConfigurationChangeParams params;
  params.settings = services::JsonValue(
      services::JsonValue::Object{{"someLang", services::JsonValue(settings)}});

  provider.updateConfiguration(params);

  const auto value = provider.getConfiguration("someLang", "prop");
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->isString());
  EXPECT_EQ(value->string(), "foo");
  ASSERT_EQ(updates.size(), 1u);
  EXPECT_EQ(updates.front().section, "someLang");
}

TEST(DefaultConfigurationProviderTest, UpdateConfigurationOverwritesLanguageSettings) {
  DefaultConfigurationProvider provider(nullptr);

  ConfigurationChangeParams params;
  params.settings = services::JsonValue(
      services::JsonValue::Object{{"someLang", services::JsonValue(
                                                   services::JsonValue::Object{{"prop", "bar"}})}});
  provider.updateConfiguration(params);

  params.settings = services::JsonValue(
      services::JsonValue::Object{{"someLang", services::JsonValue(
                                                   services::JsonValue::Object{{"prop", "bar2"}})}});
  provider.updateConfiguration(params);

  const auto value = provider.getConfiguration("someLang", "prop");
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->isString());
  EXPECT_EQ(value->string(), "bar2");
}

} // namespace
} // namespace pegium::workspace
