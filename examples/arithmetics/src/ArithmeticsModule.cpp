#include <arithmetics/services/Module.hpp>

#include <arithmetics/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/ArithmeticsCodeActionProvider.hpp"
#include "lsp/ArithmeticsFormatter.hpp"
#include "validation/ArithmeticsValidator.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace arithmetics::services {

ArithmeticsServices::ArithmeticsServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

ArithmeticsServices::ArithmeticsServices(ArithmeticsServices &&) noexcept = default;

ArithmeticsServices::~ArithmeticsServices() noexcept = default;

namespace {

std::unique_ptr<ArithmeticsServices>
create_single_language_services(
    const pegium::SharedServices &sharedServices, std::string languageId) {
  auto services = pegium::services::makeDefaultServices<ArithmeticsServices>(
      sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const arithmetics::parser::ArithmeticParser>(*services);
  services->languageMetaData.fileExtensions = {".calc"};

  services->arithmetics.validation.arithmeticsValidator =
      std::make_unique<validation::ArithmeticsValidator>();
  services->lsp.codeActionProvider =
      std::make_unique<lsp::ArithmeticsCodeActionProvider>();
  services->lsp.formatter = std::make_unique<lsp::ArithmeticsFormatter>(*services);

  validation::registerValidationChecks(*services);
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

} // namespace arithmetics::services
