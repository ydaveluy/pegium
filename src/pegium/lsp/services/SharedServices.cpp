#include <pegium/lsp/services/SharedServices.hpp>

#include <pegium/lsp/runtime/LanguageClient.hpp>

namespace pegium {

SharedServices::SharedServices() = default;
SharedServices::SharedServices(SharedServices &&) noexcept = default;
SharedServices &SharedServices::operator=(SharedServices &&) noexcept = default;
SharedServices::~SharedServices() = default;

} // namespace pegium
