#include <arithmetics/services/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "ArithmeticsCoreSetup.hpp"

namespace arithmetics {

ArithmeticsServices::ArithmeticsServices(
    const pegium::SharedCoreServices &sharedServices)
    : pegium::CoreServices(sharedServices) {}

ArithmeticsServices::ArithmeticsServices(ArithmeticsServices &&) noexcept = default;

ArithmeticsServices::~ArithmeticsServices() noexcept = default;

namespace {

std::unique_ptr<ArithmeticsServices>
create_single_language_services(
    const pegium::SharedCoreServices &sharedServices,
    std::string languageId) {
  auto services = std::make_unique<ArithmeticsServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  detail::configure_core_services(*services);
  return services;
}

} // namespace

std::unique_ptr<ArithmeticsServices>
create_language_services(
    const pegium::SharedCoreServices &sharedServices,
    std::string languageId) {
  return create_single_language_services(sharedServices, std::move(languageId));
}

bool register_language_services(
    pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_single_language_services(sharedServices, "arithmetics"));
  return true;
}

} // namespace arithmetics
