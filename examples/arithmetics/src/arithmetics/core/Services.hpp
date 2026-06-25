#pragma once

#include <memory>

#include <arithmetics/core/validation/ArithmeticsValidator.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace arithmetics {

/// Arithmetics-specific services grafted onto any pegium service container.
struct ArithmeticsAddedServices {
  std::unique_ptr<validation::ArithmeticsValidator> validator;
};

/// Core-only language services (used by the CLI and headless scenarios).
struct ArithmeticsCoreServices final : pegium::CoreServices,
                                       ArithmeticsAddedServices {
  using pegium::CoreServices::CoreServices;
};

} // namespace arithmetics
