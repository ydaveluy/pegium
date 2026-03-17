#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <pegium/services/CoreServices.hpp>

namespace pegium::services {

class ServiceRegistry {
public:
  virtual ~ServiceRegistry() noexcept = default;

  virtual bool registerServices(std::unique_ptr<CoreServices> services) = 0;

  [[nodiscard]] virtual const CoreServices *
  getServices(std::string_view uri) const = 0;
  [[nodiscard]] virtual bool hasServices(std::string_view uri) const = 0;
  [[nodiscard]] virtual std::vector<const CoreServices *> all() const = 0;

  [[nodiscard]] virtual const CoreServices *
  getServicesByLanguageId(std::string_view languageId) const = 0;
  [[nodiscard]] virtual const CoreServices *
  getServicesByFileName(std::string_view fileName) const = 0;

  virtual bool remove(std::string_view languageId) = 0;
};

} // namespace pegium::services
