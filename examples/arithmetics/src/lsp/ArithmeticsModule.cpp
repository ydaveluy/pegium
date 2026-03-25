#include <arithmetics/lsp/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "../core/ArithmeticsCoreSetup.hpp"
#include "ArithmeticsCodeActionProvider.hpp"
#include "ArithmeticsFormatter.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace arithmetics::lsp {

ArithmeticsServices::ArithmeticsServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

ArithmeticsServices::ArithmeticsServices(ArithmeticsServices &&) noexcept = default;

ArithmeticsServices::~ArithmeticsServices() noexcept = default;

namespace {

std::unique_ptr<ArithmeticsServices>
create_single_language_services(
    const pegium::SharedServices &sharedServices, std::string languageId) {
  auto services = pegium::makeDefaultServices<ArithmeticsServices>(
      sharedServices, std::move(languageId));
  detail::configure_core_services(*services);
  services->lsp.codeActionProvider =
      std::make_unique<ArithmeticsCodeActionProvider>();
  services->lsp.formatter = std::make_unique<ArithmeticsFormatter>(*services);
  return services;
}

} // namespace

std::unique_ptr<ArithmeticsServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId) {
  return create_single_language_services(sharedServices, std::move(languageId));
}

bool register_language_services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_single_language_services(sharedServices, "arithmetics"));
  return true;
}

} // namespace arithmetics::lsp
