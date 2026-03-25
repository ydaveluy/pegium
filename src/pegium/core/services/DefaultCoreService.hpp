#pragma once

#include <pegium/core/services/CoreServices.hpp>

namespace pegium {

/// Convenience base class for default services bound to one `CoreServices`.
class DefaultCoreService {
public:
  explicit DefaultCoreService(const CoreServices &services) noexcept
      : services(services) {}
  virtual ~DefaultCoreService() noexcept = default;

protected:
  const CoreServices &services;
};

} // namespace pegium
