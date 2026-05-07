#pragma once

#include <mydsl/core/Services.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace mydsl::lsp {

/// LSP-enabled language services.
struct MyDslServices final : pegium::Services,
                             mydsl::MyDslAddedServices {
  using pegium::Services::Services;
};

[[nodiscard]] inline const MyDslServices *
asMyDslServices(const pegium::Services &services) noexcept {
  return dynamic_cast<const MyDslServices *>(&services);
}

} // namespace mydsl::lsp
