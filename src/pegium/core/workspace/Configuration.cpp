#include <pegium/core/workspace/Configuration.hpp>

#include <utility>

namespace pegium::workspace {
namespace {

void apply_boolean_setting(const services::JsonValue::Object &configuration,
                           std::string_view key, bool &target) {
  const auto it = configuration.find(std::string(key));
  if (it == configuration.end()) {
    return;
  }
  const auto &value = it->second;
  if (value.isBoolean()) {
    target = value.boolean();
  }
}

[[nodiscard]] std::optional<bool>
read_optional_boolean(const services::JsonValue::Object &configuration,
                      std::string_view key) {
  const auto it = configuration.find(std::string(key));
  if (it == configuration.end() || !it->second.isBoolean()) {
    return std::nullopt;
  }
  return it->second.boolean();
}

} // namespace

bool readValidationOptions(const services::JsonValue &configuration,
                           validation::ValidationOptions &target) {
  if (!configuration.isObject()) {
    return false;
  }

  const auto &validationObject = configuration.object();
  const auto categoriesIt = validationObject.find("categories");
  if (categoriesIt != validationObject.end()) {
    const auto &categoriesValue = categoriesIt->second;
    if (categoriesValue.isArray()) {
      target.categories.clear();
      for (const auto &entry : categoriesValue.array()) {
        if (entry.isString()) {
          target.categories.push_back(entry.string());
        }
      }
    }
  }

  if (const auto stopAfterParsingErrors =
          read_optional_boolean(validationObject, "stopAfterParsingErrors");
      stopAfterParsingErrors.has_value()) {
    target.stopAfterParsingErrors = *stopAfterParsingErrors;
  }
  if (const auto stopAfterLinkingErrors =
          read_optional_boolean(validationObject, "stopAfterLinkingErrors");
      stopAfterLinkingErrors.has_value()) {
    target.stopAfterLinkingErrors = *stopAfterLinkingErrors;
  }

  return true;
}

bool readValidationOption(const services::JsonValue &configuration,
                          validation::BuildValidationOption &target) {
  if (configuration.isBoolean()) {
    target = configuration.boolean();
    return true;
  }
  if (!configuration.isObject()) {
    return false;
  }

  const auto &validationObject = configuration.object();
  if (const auto enabled = read_optional_boolean(validationObject, "enabled");
      enabled.has_value() && !*enabled) {
    target = false;
    return true;
  }

  validation::ValidationOptions options;
  if (!readValidationOptions(configuration, options)) {
    return false;
  }
  target = std::move(options);
  return true;
}

StaticConfigurationProvider::StaticConfigurationProvider(
    WorkspaceConfiguration configuration)
    : _configuration(configuration) {}

void StaticConfigurationProvider::initialize(const InitializeParams &) {}

std::future<void>
StaticConfigurationProvider::initialized(const InitializedParams &) {
  _ready = true;
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

bool StaticConfigurationProvider::isReady() const noexcept { return _ready; }

void StaticConfigurationProvider::updateConfiguration(
    const ConfigurationChangeParams &) {}

std::optional<services::JsonValue>
StaticConfigurationProvider::getConfiguration(std::string_view,
                                              std::string_view) const {
  return std::nullopt;
}

utils::ScopedDisposable
StaticConfigurationProvider::onConfigurationSectionUpdate(
    typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener) {
  return {};
}

WorkspaceConfiguration
StaticConfigurationProvider::getWorkspaceConfigurationForLanguage(
    std::string_view) const {
  return _configuration;
}

WorkspaceConfiguration
StaticConfigurationProvider::getWorkspaceConfiguration(std::string_view) const {
  return _configuration;
}

} // namespace pegium::workspace
