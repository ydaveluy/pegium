#pragma once

#include <memory>

#include <pegium/execution/TaskScheduler.hpp>
#include <pegium/syntax-tree/AstReflection.hpp>
#include <pegium/services/ServiceRegistry.hpp>
#include <pegium/workspace/Configuration.hpp>
#include <pegium/workspace/DocumentBuilder.hpp>
#include <pegium/workspace/DocumentFactory.hpp>
#include <pegium/workspace/Documents.hpp>
#include <pegium/workspace/IndexManager.hpp>
#include <pegium/workspace/TextDocuments.hpp>
#include <pegium/workspace/WorkspaceLock.hpp>
#include <pegium/workspace/WorkspaceManager.hpp>

namespace pegium::services {

struct SharedCoreServices {
  std::unique_ptr<AstReflection> astReflection;

  struct {
    std::shared_ptr<execution::TaskScheduler> taskScheduler;
  } execution;

  struct {
    std::shared_ptr<workspace::ConfigurationProvider> configurationProvider;
    std::shared_ptr<workspace::FileSystemProvider> fileSystemProvider;
    std::unique_ptr<workspace::DocumentFactory> documentFactory;
    std::unique_ptr<workspace::Documents> documents;
    std::unique_ptr<workspace::IndexManager> indexManager;
    std::unique_ptr<workspace::DocumentBuilder> documentBuilder;
    std::unique_ptr<workspace::TextDocuments> textDocuments;
    std::unique_ptr<workspace::WorkspaceLock> workspaceLock;
    std::unique_ptr<workspace::WorkspaceManager> workspaceManager;
  } workspace;

  std::unique_ptr<ServiceRegistry> serviceRegistry;
};

void installDefaultSharedCoreServices(SharedCoreServices &sharedServices);

} // namespace pegium::services
