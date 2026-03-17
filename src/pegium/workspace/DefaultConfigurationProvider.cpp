#include <pegium/workspace/DefaultConfigurationProvider.hpp>

#include <algorithm>
#include <future>
#include <utility>

#include <pegium/services/CoreServices.hpp>
#include <pegium/services/ServiceRegistry.hpp>

namespace pegium::workspace {

namespace {

std::future<void> make_ready_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

std::optional<std::string> language_id_for_uri(
    const services::ServiceRegistry *serviceRegistry, std::string_view uri) {
  if (serviceRegistry == nullptr) {
    return std::nullopt;
  }
  const auto *services = serviceRegistry->getServices(uri);
  if (services == nullptr || services->languageId.empty()) {
    return std::nullopt;
  }
  return services->languageId;
}

} // namespace

DefaultConfigurationProvider::DefaultConfigurationProvider(
    const services::ServiceRegistry *serviceRegistry)
    : _serviceRegistry(serviceRegistry) {}

void DefaultConfigurationProvider::initialize(
    const InitializeParams &params) {
  _workspaceConfigurationSupported = params.capabilities.workspaceConfiguration;
}

std::future<void> DefaultConfigurationProvider::initialized(
    const InitializedParams &params) {
  if (!_workspaceConfigurationSupported) {
    _ready = true;
    return make_ready_future();
  }

  return std::async(std::launch::async, [this, params]() mutable {
    std::vector<std::string> sections;
    sections.push_back("pegium");
    if (_serviceRegistry != nullptr) {
      for (const auto *services : _serviceRegistry->all()) {
        if (services == nullptr ||
            services->languageMetaData.languageId.empty()) {
          continue;
        }
        const auto &languageId = services->languageMetaData.languageId;
        if (std::ranges::find(sections, languageId) == sections.end()) {
          sections.push_back(languageId);
        }
      }
    }

    if (params.registerDidChangeConfiguration && !sections.empty()) {
      auto registration = params.registerDidChangeConfiguration(sections);
      if (registration.valid()) {
        registration.get();
      }
    }

    if (params.fetchConfiguration && !sections.empty()) {
      auto configuration = params.fetchConfiguration(sections);
      if (configuration.valid()) {
        auto values = configuration.get();
        const auto count = std::min(sections.size(), values.size());
        std::vector<ConfigurationSectionUpdate> updates;
        updates.reserve(count);

        {
          std::scoped_lock lock(_settingsMutex);
          for (std::size_t index = 0; index < count; ++index) {
            _settingsBySection.insert_or_assign(sections[index], values[index]);
            updates.push_back(
                {.section = sections[index], .configuration = values[index]});
          }
        }

        for (const auto &update : updates) {
          _onSectionUpdate.emit(update);
        }
      }
    }

    _ready = true;
  });
}

bool DefaultConfigurationProvider::isReady() const noexcept {
  return _ready;
}

void DefaultConfigurationProvider::updateConfiguration(
    const ConfigurationChangeParams &params) {
  if (!params.settings.isObject()) {
    return;
  }

  const auto &settings = params.settings.object();
  std::vector<ConfigurationSectionUpdate> updates;
  updates.reserve(settings.size());
  {
    std::scoped_lock lock(_settingsMutex);
    for (const auto &[section, configuration] : settings) {
      _settingsBySection.insert_or_assign(section, configuration);
      updates.push_back({.section = section, .configuration = configuration});
    }
  }

  for (const auto &update : updates) {
    _onSectionUpdate.emit(update);
  }
}

std::optional<services::JsonValue>
DefaultConfigurationProvider::getConfiguration(
    std::string_view languageId, std::string_view key) const {
  std::scoped_lock lock(_settingsMutex);
  const auto sectionIt = _settingsBySection.find(std::string(languageId));
  if (sectionIt == _settingsBySection.end() ||
      !sectionIt->second.isObject()) {
    return std::nullopt;
  }

  const auto &section = sectionIt->second.object();
  const auto keyIt = section.find(std::string(key));
  if (keyIt == section.end()) {
    return std::nullopt;
  }
  return keyIt->second;
}

utils::ScopedDisposable
DefaultConfigurationProvider::onConfigurationSectionUpdate(
    typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener
        listener) {
  return _onSectionUpdate.on(std::move(listener));
}

WorkspaceConfiguration DefaultConfigurationProvider::getWorkspaceConfiguration(
    std::string_view workspaceUri) const {
  const auto languageId = language_id_for_uri(_serviceRegistry, workspaceUri);
  if (!languageId.has_value()) {
    return {};
  }
  return getWorkspaceConfigurationForLanguage(*languageId);
}

WorkspaceConfiguration
DefaultConfigurationProvider::getWorkspaceConfigurationForLanguage(
    std::string_view languageId) const {
  WorkspaceConfiguration configuration;
  if (languageId.empty()) {
    return configuration;
  }
  std::scoped_lock lock(_settingsMutex);
  const auto sectionIt = _settingsBySection.find(std::string(languageId));
  if (sectionIt == _settingsBySection.end()) {
    return configuration;
  }
  return apply_section_overrides(configuration, sectionIt->second);
}

WorkspaceConfiguration
DefaultConfigurationProvider::apply_section_overrides(
    WorkspaceConfiguration configuration,
    const services::JsonValue &sectionConfiguration) const {
  if (!sectionConfiguration.isObject()) {
    return configuration;
  }

  const auto &section = sectionConfiguration.object();
  const auto validationIt = section.find("validation");
  if (validationIt != section.end()) {
    (void)readValidationOptions(validationIt->second, configuration.validation);
  }
  return configuration;
}

std::shared_ptr<ConfigurationProvider> make_default_configuration_provider(
    const services::ServiceRegistry *serviceRegistry) {
  return std::make_shared<DefaultConfigurationProvider>(serviceRegistry);
}

} // namespace pegium::workspace
