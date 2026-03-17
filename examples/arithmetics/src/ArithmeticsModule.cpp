#include <arithmetics/services/Module.hpp>

#include <arithmetics/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/ArithmeticsCodeActionProvider.hpp"
#include "lsp/ArithmeticsFormatter.hpp"
#include "references/ArithmeticsScopeProvider.hpp"
#include "validation/ArithmeticsValidator.hpp"

#include <pegium/references/DefaultLinker.hpp>

namespace arithmetics::services {

namespace {

std::unique_ptr<pegium::services::Services>
create_single_language_services(
    const pegium::services::SharedServices &sharedServices, std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const arithmetics::parser::ArithmeticParser>(*services);
  services->languageMetaData.fileExtensions = {".calc"};

  services->references.scopeProvider =
      std::make_unique<references::ArithmeticsScopeProvider>(*services);
  services->references.linker =
      std::make_unique<pegium::references::DefaultLinker>(*services);
  services->lsp.codeActionProvider =
      std::make_unique<lsp::ArithmeticsCodeActionProvider>();
  services->lsp.formatter = std::make_unique<lsp::ArithmeticsFormatter>(*services);

  validation::ArithmeticsValidator::registerValidationChecks(
      *services->validation.validationRegistry, *services);
  return services;
}

} // namespace

std::unique_ptr<pegium::services::Services>
create_language_services(const pegium::services::SharedServices &sharedServices,
                         std::string languageId) {
  return create_single_language_services(sharedServices, std::move(languageId));
}

bool register_language_services(pegium::services::SharedServices &sharedServices) {
  auto arithmetics = create_single_language_services(sharedServices, "arithmetics");
  auto calc = create_single_language_services(sharedServices, "calc");

  if (!sharedServices.serviceRegistry->registerServices(std::move(arithmetics))) {
    return false;
  }
  if (!sharedServices.serviceRegistry->registerServices(std::move(calc))) {
    return false;
  }
  return true;
}

} // namespace arithmetics::services
