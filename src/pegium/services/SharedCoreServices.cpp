#include <pegium/services/SharedCoreServices.hpp>

#include <pegium/services/DefaultServiceRegistry.hpp>
#include <pegium/syntax-tree/DefaultAstReflection.hpp>
#include <pegium/workspace/DefaultDocumentBuilder.hpp>
#include <pegium/workspace/DefaultDocumentFactory.hpp>
#include <pegium/workspace/DefaultDocuments.hpp>
#include <pegium/workspace/DefaultIndexManager.hpp>
#include <pegium/workspace/DefaultTextDocuments.hpp>
#include <pegium/workspace/DefaultWorkspaceLock.hpp>
#include <pegium/workspace/DefaultWorkspaceManager.hpp>

namespace pegium::services {

void installDefaultSharedCoreServices(SharedCoreServices &sharedServices) {
  if (!sharedServices.astReflection) {
    sharedServices.astReflection = std::make_unique<DefaultAstReflection>();
  }
  if (!sharedServices.execution.taskScheduler) {
    sharedServices.execution.taskScheduler =
        std::make_shared<execution::TaskScheduler>();
  }
  if (!sharedServices.workspace.textDocuments) {
    sharedServices.workspace.textDocuments =
        std::make_unique<workspace::DefaultTextDocuments>();
  }
  if (!sharedServices.serviceRegistry) {
    sharedServices.serviceRegistry = std::make_unique<DefaultServiceRegistry>();
  }
  if (auto *defaultRegistry =
          dynamic_cast<DefaultServiceRegistry *>(sharedServices.serviceRegistry.get());
      defaultRegistry != nullptr) {
    defaultRegistry->setTextDocuments(sharedServices.workspace.textDocuments.get());
  }
  if (!sharedServices.workspace.configurationProvider) {
    sharedServices.workspace.configurationProvider =
        workspace::make_default_configuration_provider(
            sharedServices.serviceRegistry.get());
  }
  if (!sharedServices.workspace.fileSystemProvider) {
    sharedServices.workspace.fileSystemProvider =
        std::make_shared<workspace::LocalFileSystemProvider>();
  }
  if (!sharedServices.workspace.documentFactory) {
    sharedServices.workspace.documentFactory =
        std::make_unique<workspace::DefaultDocumentFactory>(sharedServices);
  }
  if (!sharedServices.workspace.documents) {
    sharedServices.workspace.documents =
        std::make_unique<workspace::DefaultDocuments>(sharedServices);
  }
  if (!sharedServices.workspace.indexManager) {
    sharedServices.workspace.indexManager =
        std::make_unique<workspace::DefaultIndexManager>();
  }
  if (!sharedServices.workspace.documentBuilder) {
    sharedServices.workspace.documentBuilder =
        std::make_unique<workspace::DefaultDocumentBuilder>(sharedServices);
  }
  if (!sharedServices.workspace.workspaceLock) {
    sharedServices.workspace.workspaceLock =
        std::make_unique<workspace::DefaultWorkspaceLock>();
  }
  if (!sharedServices.workspace.workspaceManager) {
    sharedServices.workspace.workspaceManager =
        std::make_unique<workspace::DefaultWorkspaceManager>(sharedServices);
  }
}

} // namespace pegium::services
