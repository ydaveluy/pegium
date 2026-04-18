#pragma once

#include <memory>
#include <string>
#include <utility>

#include <arithmetics/core/Services.hpp>
#include <arithmetics/parser/Parser.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace arithmetics {

/// Core service overrides applied on top of pegium's default core services.
template <typename Services>
void installArithmeticsCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::ArithmeticParser>(services);
  services.languageMetaData.fileExtensions = {".calc"};
  services.validator = std::make_unique<validation::ArithmeticsValidator>();
  validation::registerValidationChecks(services);
}

/// Builds the core-only arithmetics language services.
inline std::unique_ptr<ArithmeticsCoreServices>
createArithmeticsServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId = "arithmetics") {
  auto services = std::make_unique<ArithmeticsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installArithmeticsCoreModule(*services);
  return services;
}

/// Registers the core-only arithmetics services in `sharedServices`.
inline bool
registerArithmeticsServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createArithmeticsServices(sharedServices));
  return true;
}

} // namespace arithmetics
