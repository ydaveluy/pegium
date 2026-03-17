#include <pegium/workspace/Configuration.hpp>

#include <utility>

namespace pegium::workspace {

namespace {

std::future<void> make_ready_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

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

} // namespace

bool readValidationOptions(const services::JsonValue &configuration,
                           validation::ValidationOptions &target) {
  if (!configuration.isObject()) {
    return false;
  }

  const auto &validationObject = configuration.object();
  apply_boolean_setting(validationObject, "enabled", target.enabled);

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

  return true;
}

StaticConfigurationProvider::StaticConfigurationProvider(
    WorkspaceConfiguration configuration)
    : _configuration(configuration) {}

void StaticConfigurationProvider::initialize(
    const InitializeParams &) {}

std::future<void> StaticConfigurationProvider::initialized(
    const InitializedParams &) {
  _ready = true;
  return make_ready_future();
}

bool StaticConfigurationProvider::isReady() const noexcept {
  return _ready;
}

void StaticConfigurationProvider::updateConfiguration(
    const ConfigurationChangeParams &) {}

std::optional<services::JsonValue>
StaticConfigurationProvider::getConfiguration(
    std::string_view, std::string_view) const {
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
