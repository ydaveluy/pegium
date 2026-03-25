#pragma once

#include <arithmetics/parser/Parser.hpp>

#include "validation/ArithmeticsValidator.hpp"

namespace arithmetics::detail {

template <typename TServices>
void configure_core_services(TServices &services) {
  services.parser =
      std::make_unique<const arithmetics::parser::ArithmeticParser>(services);
  services.languageMetaData.fileExtensions = {".calc"};
  services.arithmetics.validation.arithmeticsValidator =
      std::make_unique<arithmetics::validation::ArithmeticsValidator>();
  arithmetics::validation::registerValidationChecks(services);
}

} // namespace arithmetics::detail
