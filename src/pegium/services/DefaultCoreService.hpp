#pragma once

#include <pegium/services/CoreServices.hpp>

namespace pegium::services {

class DefaultCoreService {
public:
  explicit DefaultCoreService(const CoreServices &coreServices) noexcept
      : coreServices(coreServices) {}
  virtual ~DefaultCoreService() noexcept = default;

protected:
  const CoreServices &coreServices;
};

} // namespace pegium::services
