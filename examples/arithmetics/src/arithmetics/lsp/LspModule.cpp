#include <arithmetics/lsp/LspModule.hpp>

#include <utility>

#include <arithmetics/core/CoreModule.hpp>
#include <arithmetics/lsp/ArithmeticsCodeActionProvider.hpp>
#include <arithmetics/lsp/ArithmeticsFormatter.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace arithmetics {

void installArithmeticsLspModule(ArithmeticsServices &services) {
  services.lsp.codeActionProvider =
      std::make_unique<ArithmeticsCodeActionProvider>();
  services.lsp.formatter = std::make_unique<ArithmeticsFormatter>(services);
}

std::unique_ptr<ArithmeticsServices>
createArithmeticsLspServices(const pegium::SharedServices &sharedServices,
                             std::string languageId) {
  auto services = pegium::makeDefaultServices<ArithmeticsServices>(
      sharedServices, std::move(languageId));
  // The container is-a CoreServices and is-a ArithmeticsAddedServices, so it
  // binds both parameters of the (non-template) core wiring.
  installArithmeticsCoreModule(*services, *services);
  installArithmeticsLspModule(*services);
  return services;
}

bool registerArithmeticsLspServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createArithmeticsLspServices(sharedServices));
  return true;
}

} // namespace arithmetics
