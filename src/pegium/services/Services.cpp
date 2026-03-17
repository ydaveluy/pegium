#include <pegium/services/Services.hpp>

#include <pegium/lsp/DefaultLspModule.hpp>

#include <utility>

namespace pegium::services {

Services::Services(const SharedServices &sharedServices)
    : CoreServices(sharedServices), sharedServices(sharedServices) {}

Services::~Services() noexcept = default;

std::unique_ptr<Services>
makeDefaultServices(const SharedServices &sharedServices, std::string languageId) {
  auto services = std::make_unique<Services>(sharedServices);
  services->languageId = std::move(languageId);
  services->languageMetaData.languageId = services->languageId;
  installDefaultCoreServices(*services);
  lsp::installDefaultLspServices(*services);
  if (services->languageMetaData.languageId.empty()) {
    services->languageMetaData.languageId = services->languageId;
  }
  return services;
}

} // namespace pegium::services
