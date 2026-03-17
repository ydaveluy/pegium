#pragma once

#include <pegium/services/SharedCoreServices.hpp>

namespace pegium::services {

class DefaultSharedCoreService {
public:
  explicit DefaultSharedCoreService(SharedCoreServices &sharedCoreServices)
      : sharedCoreServices(sharedCoreServices) {}
  virtual ~DefaultSharedCoreService() noexcept = default;

protected:
  SharedCoreServices &sharedCoreServices;
};

} // namespace pegium::services
