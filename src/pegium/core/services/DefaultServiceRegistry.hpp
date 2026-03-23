#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pegium/core/services/DefaultSharedCoreService.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>

namespace pegium::services {

/// Default registry mapping URIs to their language service container.
class DefaultServiceRegistry : public ServiceRegistry,
                               protected DefaultSharedCoreService {
public:
  explicit DefaultServiceRegistry(
      const SharedCoreServices &sharedServices) noexcept
      : DefaultSharedCoreService(sharedServices) {}

  void registerServices(std::unique_ptr<CoreServices> services) override;

  [[nodiscard]] const CoreServices &
  getServices(std::string_view uri) const override;
  [[nodiscard]] const CoreServices *
  findServices(std::string_view uri) const noexcept override;
  [[nodiscard]] std::vector<const CoreServices *> all() const override;

private:
  [[nodiscard]] const CoreServices *
  findServicesLocked(std::string_view normalizedUri,
                     std::string *languageId = nullptr) const;
  [[nodiscard]] const CoreServices *
  lookupByExtension(std::string_view extension) const;
  [[nodiscard]] const CoreServices *
  lookupByFileName(std::string_view fileName) const;
  void removeLanguageMappingsLocked(std::string_view languageId,
                                   const CoreServices &services);
  void addLanguageMappingsLocked(std::string_view languageId,
                                 const CoreServices &services);

  mutable std::mutex _mutex;
  utils::TransparentStringMap<std::unique_ptr<CoreServices>> _servicesByLanguageId;
  std::vector<std::string> _registrationOrder;
  utils::TransparentStringMap<std::string> _languageIdByExtension;
  utils::TransparentStringMap<std::string> _languageIdByFileName;
};

} // namespace pegium::services
