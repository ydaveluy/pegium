#pragma once

#include <requirements/core/Services.hpp>
#include <pegium/core/services/ServiceAccess.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace requirements::lsp {

/// LSP-enabled requirements language services.
struct RequirementsServices final : pegium::Services,
                                    requirements::RequirementsAddedServices {
  using pegium::Services::Services;
};

/// LSP-enabled tests language services.
struct TestsServices final : pegium::Services, requirements::TestsAddedServices {
  using pegium::Services::Services;
};

[[nodiscard]] inline const RequirementsServices *
asRequirementsServices(const pegium::Services &services) noexcept {
  return pegium::service_cast<RequirementsServices>(services);
}

[[nodiscard]] inline const TestsServices *
asTestsServices(const pegium::Services &services) noexcept {
  return pegium::service_cast<TestsServices>(services);
}

} // namespace requirements::lsp
