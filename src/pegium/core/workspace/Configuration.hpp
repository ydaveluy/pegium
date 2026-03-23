#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/core/services/JsonValue.hpp>
#include <pegium/core/utils/Disposable.hpp>
#include <pegium/core/utils/Event.hpp>
#include <pegium/core/validation/ValidationOptions.hpp>
#include <pegium/core/workspace/WorkspaceProtocol.hpp>

namespace pegium::services {
class ServiceRegistry;
}

namespace pegium::workspace {

/// Effective workspace-level options consumed by core services.
struct WorkspaceConfiguration {
  validation::BuildValidationOption validation;
};

/// Reads full validation options from one configuration object.
[[nodiscard]] bool
readValidationOptions(const services::JsonValue &configuration,
                      validation::ValidationOptions &target);
/// Reads either a boolean or detailed validation options from one configuration object.
[[nodiscard]] bool
readValidationOption(const services::JsonValue &configuration,
                     validation::BuildValidationOption &target);

/// Provides language and workspace configuration to core services.
class ConfigurationProvider {
public:
  virtual ~ConfigurationProvider() noexcept = default;

  /// Records initialize-time client capabilities.
  virtual void initialize(const InitializeParams &params) = 0;
  /// Performs asynchronous post-initialize configuration setup.
  [[nodiscard]] virtual std::future<void>
  initialized(const InitializedParams &params) = 0;
  /// Returns whether the provider finished its startup work.
  [[nodiscard]] virtual bool isReady() const noexcept = 0;

  /// Applies a configuration change notification from the client.
  virtual void updateConfiguration(const ConfigurationChangeParams &params) = 0;
  /// Returns one configuration section value for `languageId` and `key`.
  [[nodiscard]] virtual std::optional<services::JsonValue>
  getConfiguration(std::string_view languageId, std::string_view key) const = 0;

  /// One emitted configuration section update.
  struct ConfigurationSectionUpdate {
    std::string section;
    services::JsonValue configuration;
  };
  /// Subscribes to configuration section updates.
  [[nodiscard]] virtual utils::ScopedDisposable onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener
          listener) = 0;

  /// Returns the effective configuration for a language identifier.
  [[nodiscard]] virtual WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(std::string_view languageId) const = 0;
  /// Returns the effective configuration for a workspace URI.
  virtual WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view workspaceUri) const = 0;
};

/// Static configuration provider used when no client-side configuration exists.
class StaticConfigurationProvider final : public ConfigurationProvider {
public:
  explicit StaticConfigurationProvider(
      WorkspaceConfiguration configuration = {});

  void initialize(const InitializeParams &params) override;
  [[nodiscard]] std::future<void>
  initialized(const InitializedParams &params) override;
  [[nodiscard]] bool isReady() const noexcept override;

  void updateConfiguration(const ConfigurationChangeParams &params) override;
  [[nodiscard]] std::optional<services::JsonValue>
  getConfiguration(std::string_view languageId,
                   std::string_view key) const override;

  [[nodiscard]] utils::ScopedDisposable onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener
          listener) override;

  WorkspaceConfiguration getWorkspaceConfigurationForLanguage(
      std::string_view languageId) const override;
  WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view workspaceUri) const override;

private:
  WorkspaceConfiguration _configuration;
  bool _ready = true;
};

} // namespace pegium::workspace
