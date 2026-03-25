#pragma once

#include <memory>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/core/execution/TaskScheduler.hpp>
#include <pegium/core/syntax-tree/AstReflection.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/workspace/Configuration.hpp>
#include <pegium/core/workspace/DocumentBuilder.hpp>
#include <pegium/core/workspace/DocumentFactory.hpp>
#include <pegium/core/workspace/Documents.hpp>
#include <pegium/core/workspace/IndexManager.hpp>
#include <pegium/core/workspace/TextDocumentProvider.hpp>
#include <pegium/core/workspace/WorkspaceLock.hpp>
#include <pegium/core/workspace/WorkspaceManager.hpp>

namespace pegium {

/// Shared execution services reused by every language.
struct SharedCoreExecutionServices {
  // Shared core service; installed by default for standard Pegium setups.
  std::shared_ptr<execution::TaskScheduler> taskScheduler;
};

/// Shared workspace services reused by every language.
struct SharedCoreWorkspaceServices {
  // Shared core service; installed by default for standard Pegium setups.
  std::shared_ptr<workspace::ConfigurationProvider> configurationProvider;
  // Shared core service; installed implicitly by default and overridable.
  std::shared_ptr<workspace::FileSystemProvider> fileSystemProvider;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<workspace::DocumentFactory> documentFactory;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<workspace::Documents> documents;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<workspace::IndexManager> indexManager;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<workspace::DocumentBuilder> documentBuilder;
  // Optional shared core provider; published by the shared LSP text manager.
  std::shared_ptr<workspace::TextDocumentProvider> textDocuments;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<workspace::WorkspaceLock> workspaceLock;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<workspace::WorkspaceManager> workspaceManager;
};

/// Root shared service container used by all registered languages.
struct SharedCoreServices {
  // Shared core service; installed by default for standard Pegium setups.
  std::shared_ptr<observability::ObservabilitySink> observabilitySink;
  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<AstReflection> astReflection;

  SharedCoreExecutionServices execution;
  SharedCoreWorkspaceServices workspace;

  // Shared core service; installed by default for standard Pegium setups.
  std::unique_ptr<ServiceRegistry> serviceRegistry;

  SharedCoreServices() = default;
  SharedCoreServices(SharedCoreServices &&) noexcept = default;
  SharedCoreServices &operator=(SharedCoreServices &&) noexcept = default;
  SharedCoreServices(const SharedCoreServices &) = delete;
  SharedCoreServices &operator=(const SharedCoreServices &) = delete;
  virtual ~SharedCoreServices() noexcept = default;
};

/// Installs the default shared core services into `sharedServices`.
void installDefaultSharedCoreServices(SharedCoreServices &sharedServices);

} // namespace pegium
