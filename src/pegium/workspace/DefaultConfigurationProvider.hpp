#pragma once

#include <mutex>
#include <unordered_map>

#include <pegium/workspace/Configuration.hpp>

namespace pegium::workspace {

class DefaultConfigurationProvider final : public ConfigurationProvider {
public:
  explicit DefaultConfigurationProvider(
      const services::ServiceRegistry *serviceRegistry = nullptr);

  void initialize(const InitializeParams &params) override;
  [[nodiscard]] std::future<void>
  initialized(const InitializedParams &params) override;
  [[nodiscard]] bool isReady() const noexcept override;

  void updateConfiguration(const ConfigurationChangeParams &params) override;
  [[nodiscard]] std::optional<services::JsonValue>
  getConfiguration(std::string_view languageId,
                   std::string_view key) const override;

  [[nodiscard]] utils::ScopedDisposable
  onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener
          listener) override;

  WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(
      std::string_view languageId) const override;
  WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view workspaceUri) const override;

private:
  WorkspaceConfiguration apply_section_overrides(
      WorkspaceConfiguration configuration,
      const services::JsonValue &sectionConfiguration) const;

  const services::ServiceRegistry *_serviceRegistry = nullptr;
  mutable std::mutex _settingsMutex;
  std::unordered_map<std::string, services::JsonValue> _settingsBySection;
  utils::EventEmitter<ConfigurationSectionUpdate> _onSectionUpdate;
  bool _workspaceConfigurationSupported = false;
  bool _ready = false;
};

} // namespace pegium::workspace
