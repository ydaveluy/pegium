#pragma once

#include <pegium/services/DefaultSharedCoreService.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::services {

class DefaultSharedLspService : protected DefaultSharedCoreService {
public:
  explicit DefaultSharedLspService(SharedServices &sharedServices)
      : DefaultSharedCoreService(sharedServices), sharedServices(sharedServices) {}
  virtual ~DefaultSharedLspService() noexcept = default;

protected:
  SharedServices &sharedServices;
};

} // namespace pegium::services
