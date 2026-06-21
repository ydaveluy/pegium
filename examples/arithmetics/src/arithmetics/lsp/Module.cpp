#include <arithmetics/lsp/Module.hpp>

#include <utility>

#include <arithmetics/core/validation/ArithmeticsValidator.hpp>
#include <arithmetics/lsp/ArithmeticsCodeActionProvider.hpp>
#include <arithmetics/lsp/ArithmeticsFormatter.hpp>
#include <arithmetics/core/Parser.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace arithmetics {

namespace {
template <typename Services>
void applyArithmeticsCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::ArithmeticParser>(services);
  services.languageMetaData.fileExtensions = {".calc"};
  services.validator = std::make_unique<validation::ArithmeticsValidator>();
  validation::registerValidationChecks(services);
}
} // namespace

void installArithmeticsCoreModule(lsp::ArithmeticsServices &services) {
  applyArithmeticsCoreModule(services);
}

} // namespace arithmetics

namespace arithmetics::lsp {

void installArithmeticsLspModule(ArithmeticsServices &services) {
  services.lsp.codeActionProvider =
      std::make_unique<ArithmeticsCodeActionProvider>();
  services.lsp.formatter = std::make_unique<ArithmeticsFormatter>(services);
}

std::unique_ptr<ArithmeticsServices>
createArithmeticsServices(const pegium::SharedServices &sharedServices,
                          std::string languageId) {
  auto services = pegium::makeDefaultServices<ArithmeticsServices>(
      sharedServices, std::move(languageId));
  arithmetics::installArithmeticsCoreModule(*services);
  installArithmeticsLspModule(*services);
  return services;
}

bool registerArithmeticsServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createArithmeticsServices(sharedServices));
  return true;
}

} // namespace arithmetics::lsp
