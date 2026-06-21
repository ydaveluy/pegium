#include <arithmetics/core/Module.hpp>

#include <utility>

#include <arithmetics/core/validation/ArithmeticsValidator.hpp>
#include <arithmetics/core/Parser.hpp>

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

void installArithmeticsCoreModule(ArithmeticsCoreServices &services) {
  applyArithmeticsCoreModule(services);
}

std::unique_ptr<ArithmeticsCoreServices>
createArithmeticsServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId) {
  auto services = std::make_unique<ArithmeticsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installArithmeticsCoreModule(*services);
  return services;
}

bool registerArithmeticsServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createArithmeticsServices(sharedServices));
  return true;
}

} // namespace arithmetics
