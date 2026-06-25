#pragma once

namespace pegium {

/// Downcasts a service-container reference to a derived container type,
/// returning `nullptr` when the dynamic type does not match.
///
/// This is the single supported idiom for the case where code holds only a base
/// container reference (typically a `const pegium::CoreServices &` or
/// `const pegium::Services &` handed to a framework-invoked service) and must
/// recover the language-specific container that carries its added services:
///
/// ```cpp
/// const auto *added =
///     pegium::service_cast<MyAddedServices>(services);
/// if (added == nullptr) {
///   return nullptr; // not this language's container
/// }
/// use(*added->myService);
/// ```
///
/// `Base` is deduced; spell out only the `Derived` target. The cast is a plain
/// `dynamic_cast`, so it works for any base/derived service-container pair
/// (these are not `AstNode`-derived, so `ast_ptr_cast` does not apply).
template <typename Derived, typename Base>
[[nodiscard]] const Derived *service_cast(const Base &services) noexcept {
  return dynamic_cast<const Derived *>(&services);
}

/// Mixin that captures a typed, non-owning back-reference to a language-specific
/// services bundle — typically an `…AddedServices` base shared by a language's
/// core and LSP containers.
///
/// A custom service inherits this alongside its Pegium base and constructs both
/// from the same container at the `make_unique` site, so it reaches its own
/// added services through a direct, statically-typed access
/// (`languageServices.myService`) instead of a runtime `service_cast` plus
/// null-check. Use it when a framework-invoked service needs *its own*
/// language's siblings; keep `service_cast` for the rarer cross-language probe.
///
/// The reference is exactly as lifetime-safe as the Pegium base's own
/// back-reference: service containers are non-movable and heap-pinned for their
/// whole lifetime. The member is named `languageServices` (not `services`) so it
/// never collides with the `services` member a Pegium base already provides.
template <typename LanguageServices> class LanguageServiceMixin {
protected:
  explicit LanguageServiceMixin(const LanguageServices &services) noexcept
      : languageServices(services) {}

  const LanguageServices &languageServices;
};

} // namespace pegium
