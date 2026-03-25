#pragma once

#include <memory>

#include <pegium/core/services/CoreServices.hpp>

namespace arithmetics::validation {
class ArithmeticsValidator;
}

namespace arithmetics {

struct ArithmeticsValidationServices {
  std::unique_ptr<validation::ArithmeticsValidator> arithmeticsValidator;
};

struct ArithmeticsAddedServices {
  ArithmeticsValidationServices validation;
};

struct ArithmeticsServices final : pegium::CoreServices {
  explicit ArithmeticsServices(
      const pegium::SharedCoreServices &sharedServices);
  ArithmeticsServices(ArithmeticsServices &&) noexcept;
  ArithmeticsServices &operator=(ArithmeticsServices &&) noexcept = delete;
  ArithmeticsServices(const ArithmeticsServices &) = delete;
  ArithmeticsServices &operator=(const ArithmeticsServices &) = delete;
  ~ArithmeticsServices() noexcept override;

  ArithmeticsAddedServices arithmetics;
};

[[nodiscard]] inline const ArithmeticsServices *
as_arithmetics_services(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const ArithmeticsServices *>(&services);
}

} // namespace arithmetics
