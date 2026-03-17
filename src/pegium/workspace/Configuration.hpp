#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pegium/services/JsonValue.hpp>
#include <pegium/utils/Disposable.hpp>
#include <pegium/utils/Event.hpp>
#include <pegium/validation/ValidationOptions.hpp>
#include <pegium/workspace/WorkspaceProtocol.hpp>

namespace pegium::services {
class ServiceRegistry;
}

namespace pegium::workspace {

struct WorkspaceConfiguration {
  validation::ValidationOptions validation{};
};

[[nodiscard]] bool
readValidationOptions(const services::JsonValue &configuration,
                      validation::ValidationOptions &target);

class ConfigurationProvider {
public:
  virtual ~ConfigurationProvider() noexcept = default;

  virtual void initialize(const InitializeParams &params) = 0;
  [[nodiscard]] virtual std::future<void>
  initialized(const InitializedParams &params) = 0;
  [[nodiscard]] virtual bool isReady() const noexcept = 0;

  virtual void updateConfiguration(const ConfigurationChangeParams &params) = 0;
  [[nodiscard]] virtual std::optional<services::JsonValue>
  getConfiguration(std::string_view languageId,
                   std::string_view key) const = 0;

  struct ConfigurationSectionUpdate {
    std::string section;
    services::JsonValue configuration;
  };
  [[nodiscard]] virtual utils::ScopedDisposable
  onConfigurationSectionUpdate(
      typename utils::EventEmitter<ConfigurationSectionUpdate>::Listener
          listener) = 0;

  [[nodiscard]] virtual WorkspaceConfiguration
  getWorkspaceConfigurationForLanguage(std::string_view languageId) const = 0;
  virtual WorkspaceConfiguration
  getWorkspaceConfiguration(std::string_view workspaceUri) const = 0;
};

class StaticConfigurationProvider final
    : public ConfigurationProvider {
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
  WorkspaceConfiguration _configuration;
  bool _ready = true;
};

std::shared_ptr<ConfigurationProvider>
make_default_configuration_provider(
    const services::ServiceRegistry *serviceRegistry = nullptr);

} // namespace pegium::workspace
