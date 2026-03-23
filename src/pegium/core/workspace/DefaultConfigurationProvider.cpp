#include <pegium/core/workspace/DefaultConfigurationProvider.hpp>

#include <algorithm>
#include <future>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>

namespace pegium::workspace {

DefaultConfigurationProvider::DefaultConfigurationProvider(
    const services::SharedCoreServices &sharedServices)
    : services::DefaultSharedCoreService(sharedServices) {}

void DefaultConfigurationProvider::initialize(const InitializeParams &params) {
  _workspaceConfigurationSupported = params.capabilities.workspaceConfiguration;
}

std::future<void>
DefaultConfigurationProvider::initialized(const InitializedParams &params) {
  if (!_workspaceConfigurationSupported) {
    _ready = true;
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  return std::async(std::launch::async, [this, params]() mutable {
    std::vector<std::string> sections;
    sections.push_back("pegium");
    for (const auto *services : shared.serviceRegistry->all()) {
      const auto &languageId = services->languageMetaData.languageId;
      if (std::ranges::find(sections, languageId) == sections.end()) {
        sections.push_back(languageId);
      }
    }

    if (params.registerDidChangeConfiguration) {
      auto registration = params.registerDidChangeConfiguration(sections);
      if (registration.valid()) {
        registration.get();
      }
    }

    if (params.fetchConfiguration) {
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

bool DefaultConfigurationProvider::isReady() const noexcept { return _ready; }

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
DefaultConfigurationProvider::getConfiguration(std::string_view languageId,
                                               std::string_view key) const {
  std::scoped_lock lock(_settingsMutex);
  const auto sectionIt = _settingsBySection.find(std::string(languageId));
  if (sectionIt == _settingsBySection.end() || !sectionIt->second.isObject()) {
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
  if (const auto *services = shared.serviceRegistry->findServices(workspaceUri);
      services != nullptr) {
    return getWorkspaceConfigurationForLanguage(
        services->languageMetaData.languageId);
  }
  return {};
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

WorkspaceConfiguration DefaultConfigurationProvider::apply_section_overrides(
    WorkspaceConfiguration configuration,
    const services::JsonValue &sectionConfiguration) const {
  if (!sectionConfiguration.isObject()) {
    return configuration;
  }

  const auto &section = sectionConfiguration.object();
  const auto validationIt = section.find("validation");
  if (validationIt != section.end()) {
    (void)readValidationOption(validationIt->second, configuration.validation);
  }
  return configuration;
}

} // namespace pegium::workspace
