#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pegium/documentation/CommentProvider.hpp>
#include <pegium/documentation/DocumentationProvider.hpp>
#include <pegium/parser/Parser.hpp>
#include <pegium/references/Linker.hpp>
#include <pegium/references/NameProvider.hpp>
#include <pegium/references/References.hpp>
#include <pegium/references/ScopeComputation.hpp>
#include <pegium/references/ScopeProvider.hpp>
#include <pegium/validation/DocumentValidator.hpp>
#include <pegium/validation/ValidationRegistry.hpp>
#include <pegium/workspace/AstNodeDescriptionProvider.hpp>
#include <pegium/workspace/ReferenceDescriptionProvider.hpp>

namespace pegium::services {

struct SharedCoreServices;

struct LanguageMetaData {
  std::string languageId;
  std::vector<std::string> fileExtensions;
  std::vector<std::string> fileNames;
};

struct DocumentationServices {
  std::unique_ptr<documentation::CommentProvider> commentProvider;
  std::unique_ptr<documentation::DocumentationProvider> documentationProvider;
};

struct ReferenceServices {
  std::unique_ptr<references::NameProvider> nameProvider;
  std::unique_ptr<references::ScopeProvider> scopeProvider;
  std::unique_ptr<references::References> references;
  std::unique_ptr<references::ScopeComputation> scopeComputation;
  std::unique_ptr<references::Linker> linker;
};

struct ValidationServices {
  std::unique_ptr<validation::ValidationRegistry> validationRegistry;
  std::unique_ptr<validation::DocumentValidator> documentValidator;
};

struct WorkspaceServices {
  std::unique_ptr<workspace::AstNodeDescriptionProvider>
      astNodeDescriptionProvider;
  std::unique_ptr<workspace::ReferenceDescriptionProvider>
      referenceDescriptionProvider;
};

struct CoreServices {
  using MetaData = LanguageMetaData;

  explicit CoreServices(const SharedCoreServices &sharedServices)
      : shared(sharedServices) {}
  virtual ~CoreServices() noexcept = default;

  [[nodiscard]] virtual bool isComplete() const noexcept;

  std::string languageId;
  LanguageMetaData languageMetaData;
  const SharedCoreServices &shared;

  DocumentationServices documentation;
  std::unique_ptr<const parser::Parser> parser;
  ReferenceServices references;
  ValidationServices validation;
  WorkspaceServices workspace;
};

void installDefaultCoreServices(CoreServices &services);

} // namespace pegium::services
