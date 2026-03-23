#pragma once

#include <memory>
#include <string>
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
#include <pegium/core/workspace/AstNodeLocator.hpp>
#include <pegium/core/workspace/ReferenceDescriptionProvider.hpp>

namespace pegium::services {

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
  std::unique_ptr<workspace::AstNodeLocator> astNodeLocator;
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
  CoreServices(CoreServices &&) noexcept = default;
  CoreServices &operator=(CoreServices &&) noexcept = delete;
  CoreServices(const CoreServices &) = delete;
  CoreServices &operator=(const CoreServices &) = delete;
  virtual ~CoreServices() noexcept = default;

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

} // namespace pegium::services
