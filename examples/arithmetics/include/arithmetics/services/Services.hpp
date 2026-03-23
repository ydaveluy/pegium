#pragma once

#include <memory>

#include <pegium/lsp/services/Services.hpp>

namespace arithmetics::services::validation {
class ArithmeticsValidator;
}

namespace arithmetics::services {

struct ArithmeticsValidationServices {
  std::unique_ptr<validation::ArithmeticsValidator> arithmeticsValidator;
};

struct ArithmeticsAddedServices {
  ArithmeticsValidationServices validation;
};

struct ArithmeticsServices final : pegium::Services {
  explicit ArithmeticsServices(
      const pegium::SharedServices &sharedServices);
  ArithmeticsServices(ArithmeticsServices &&) noexcept;
  ArithmeticsServices &operator=(ArithmeticsServices &&) noexcept = delete;
  ArithmeticsServices(const ArithmeticsServices &) = delete;
  ArithmeticsServices &operator=(const ArithmeticsServices &) = delete;
  ~ArithmeticsServices() noexcept override;

  ArithmeticsAddedServices arithmetics;
};

[[nodiscard]] inline const ArithmeticsServices *
as_arithmetics_services(const pegium::services::CoreServices &services) noexcept {
  return dynamic_cast<const ArithmeticsServices *>(&services);
}

[[nodiscard]] inline const ArithmeticsServices *
as_arithmetics_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const ArithmeticsServices *>(&services);
}

} // namespace arithmetics::services
