#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <pegium/core/documentation/CommentProvider.hpp>
#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/references/Linker.hpp>
#include <pegium/core/references/NameProvider.hpp>
#include <pegium/core/references/References.hpp>
#include <pegium/core/references/ScopeComputation.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/core/validation/DocumentValidator.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>
#include <pegium/core/workspace/AstNodeDescriptionProvider.hpp>
#include <pegium/core/workspace/ReferenceDescriptionProvider.hpp>

namespace pegium {

struct SharedCoreServices;

/// Language-level metadata used for URI-to-language resolution.
struct LanguageMetaData {
  std::string languageId;
  std::vector<std::string> fileExtensions;
  std::vector<std::string> fileNames;
};

/// Documentation-related services owned by one language.
struct DocumentationServices {
  // Required for a complete language service; installed by default.
  std::unique_ptr<documentation::CommentProvider> commentProvider;
  // Required for a complete language service; installed by default.
  std::unique_ptr<documentation::DocumentationProvider> documentationProvider;
};

/// Cross-reference services owned by one language.
struct ReferenceServices {
  // Required for a complete language service; installed by default.
  std::unique_ptr<references::NameProvider> nameProvider;
  // Required for a complete language service; installed by default.
  std::unique_ptr<references::ScopeProvider> scopeProvider;
  // Required for a complete language service; installed by default.
  std::unique_ptr<references::References> references;
  // Required for a complete language service; installed by default.
  std::unique_ptr<references::ScopeComputation> scopeComputation;
  // Required for a complete language service; installed by default.
  std::unique_ptr<references::Linker> linker;
};

/// Validation services owned by one language.
struct ValidationServices {
  // Required for a complete language service; installed by default.
  std::unique_ptr<validation::ValidationRegistry> validationRegistry;
  // Required for a complete language service; installed by default.
  std::unique_ptr<validation::DocumentValidator> documentValidator;
};

/// Workspace-facing services owned by one language.
struct WorkspaceServices {
  // Required for a complete language service; installed by default.
  std::unique_ptr<workspace::AstNodeDescriptionProvider>
      astNodeDescriptionProvider;
  // Required for a complete language service; installed by default.
  std::unique_ptr<workspace::ReferenceDescriptionProvider>
      referenceDescriptionProvider;
};

/// Root service container for one language configuration.
struct CoreServices {
  using MetaData = LanguageMetaData;

  explicit CoreServices(const SharedCoreServices &sharedServices)
      : shared(sharedServices) {}
  // Non-movable: installed default services hold a back-reference to this
  // CoreServices (DefaultCoreService::services), so a member-wise move would
  // leave them dangling. CoreServices is only ever heap-owned via unique_ptr,
  // so the move ctor is never needed. (Matches pegium::Services.)
  CoreServices(CoreServices &&) noexcept = delete;
  CoreServices &operator=(CoreServices &&) noexcept = delete;
  CoreServices(const CoreServices &) = delete;
  CoreServices &operator=(const CoreServices &) = delete;
  virtual ~CoreServices() noexcept = default;

  /// Returns the field path of the first required service that is missing
  /// (e.g. `"parser"` or `"references.scopeProvider"`), or `std::nullopt` when
  /// the container is complete. This is the single source of truth for the
  /// required-service set; `isComplete()` is derived from it.
  [[nodiscard]] virtual std::optional<std::string_view>
  firstMissingService() const noexcept;

  [[nodiscard]] virtual bool isComplete() const noexcept;

  LanguageMetaData languageMetaData;
  const SharedCoreServices &shared;

  DocumentationServices documentation;
  // Required for a complete language service, but not installed by
  // installDefaultCoreServices(...).
  std::unique_ptr<const parser::Parser> parser;
  ReferenceServices references;
  ValidationServices validation;
  WorkspaceServices workspace;
};

/// Installs the default language-level services into `services`.
///
/// Shared defaults must already be installed on `services.shared`.
void installDefaultCoreServices(CoreServices &services);

/// Builds one core-only language service container with the default core
/// services installed — the headless counterpart of `makeDefaultServices`.
///
/// Allocate, set the language id, and install the defaults in one call; your
/// install-module then adds the parser and overrides. The template argument
/// defaults to the base `CoreServices`; pass your own derived type so the
/// container can hold language-specific members.
template <typename TServices = CoreServices>
[[nodiscard]] std::unique_ptr<TServices>
makeDefaultCoreServices(const SharedCoreServices &sharedServices,
                        std::string languageId) {
  static_assert(std::is_base_of_v<CoreServices, TServices>,
                "TServices must derive from pegium::CoreServices.");
  static_assert(
      std::is_constructible_v<TServices, const SharedCoreServices &>,
      "TServices must be constructible from const pegium::SharedCoreServices&.");

  auto services = std::make_unique<TServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  installDefaultCoreServices(*services);
  return services;
}

} // namespace pegium
