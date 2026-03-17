#include <pegium/services/SharedServices.hpp>

#include <pegium/lsp/DefaultLspModule.hpp>

namespace pegium::services {

SharedServices::SharedServices() {
  installDefaultSharedCoreServices(*this);
  lsp::installDefaultSharedLspServices(*this);
}

SharedServices::SharedServices(NoDefaultsTag) {}

SharedServices::SharedServices(SharedServices &&) noexcept = default;
SharedServices &SharedServices::operator=(SharedServices &&) noexcept = default;

SharedServices::~SharedServices() = default;

} // namespace pegium::services
