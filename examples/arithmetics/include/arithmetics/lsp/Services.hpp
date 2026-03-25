#pragma once

#include <memory>

#include <arithmetics/services/Services.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace arithmetics::lsp {

struct ArithmeticsServices final : pegium::Services {
  explicit ArithmeticsServices(
      const pegium::SharedServices &sharedServices);
  ArithmeticsServices(ArithmeticsServices &&) noexcept;
  ArithmeticsServices &operator=(ArithmeticsServices &&) noexcept = delete;
  ArithmeticsServices(const ArithmeticsServices &) = delete;
  ArithmeticsServices &operator=(const ArithmeticsServices &) = delete;
  ~ArithmeticsServices() noexcept override;

  arithmetics::ArithmeticsAddedServices arithmetics;
};

[[nodiscard]] inline const ArithmeticsServices *
as_arithmetics_services(const pegium::Services &services) noexcept {
  return dynamic_cast<const ArithmeticsServices *>(&services);
}

} // namespace arithmetics::lsp
