#pragma once

#include <pegium/core/services/CoreServices.hpp>

namespace mydsl {

/// Language-specific services grafted onto a Pegium service container.
/// Add your own services here as the language grows (e.g. a custom name
/// provider, a language-specific evaluator, etc.).
struct MyDslAddedServices {};

/// Core-only language services (used by the CLI and headless scenarios).
struct MyDslCoreServices final : pegium::CoreServices,
                                 MyDslAddedServices {
  using pegium::CoreServices::CoreServices;
};

[[nodiscard]] inline const MyDslCoreServices *
asMyDslCoreServices(const pegium::CoreServices &services) noexcept {
  return dynamic_cast<const MyDslCoreServices *>(&services);
}

} // namespace mydsl
