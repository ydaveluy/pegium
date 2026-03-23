#include <pegium/lsp/services/Services.hpp>

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

Services::Services(const SharedServices &sharedServices)
    : services::CoreServices(sharedServices), shared(sharedServices) {}

Services::~Services() noexcept = default;

} // namespace pegium
