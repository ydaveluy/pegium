#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <pegium/services/DefaultSharedCoreService.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/WorkspaceManager.hpp>

namespace pegium::workspace {

class DefaultWorkspaceManager : public WorkspaceManager,
                                protected services::DefaultSharedCoreService {
public:
  explicit DefaultWorkspaceManager(services::SharedCoreServices &sharedServices);

  [[nodiscard]] BuildOptions &initialBuildOptions() override;
  [[nodiscard]] const BuildOptions &initialBuildOptions() const override;
  [[nodiscard]] bool isReady() const noexcept override;
  void waitUntilReady(utils::CancellationToken cancelToken = {}) const override;
  [[nodiscard]] std::vector<WorkspaceFolder> workspaceFolders() const override;

  void initialize(const InitializeParams &params) override;
  [[nodiscard]] std::future<void>
  initialized(const InitializedParams &params,
              utils::CancellationToken cancelToken = {}) override;
  [[nodiscard]] std::future<void>
  initializeWorkspace(std::span<const WorkspaceFolder> workspaceFolders,
                      utils::CancellationToken cancelToken = {}) override;
  [[nodiscard]] std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const override;
  [[nodiscard]] bool
  shouldIncludeEntry(std::string_view workspaceUri, std::string_view path,
                     bool isDirectory) const override;

protected:
  [[nodiscard]] virtual std::string
  getRootFolder(const WorkspaceFolder &workspaceFolder) const;

  virtual void loadAdditionalDocuments(
      std::span<const WorkspaceFolder> workspaceFolders,
      const std::function<void(std::shared_ptr<Document>)> &collector,
      utils::CancellationToken cancelToken);

  virtual void loadWorkspaceDocuments(
      std::span<const std::string> workspaceFileUris,
      const std::function<void(std::shared_ptr<Document>)> &collector,
      utils::CancellationToken cancelToken);

  BuildOptions _initialBuildOptions;

private:
  [[nodiscard]] std::vector<std::shared_ptr<Document>> performStartup(
      std::span<const WorkspaceFolder> workspaceFolders,
      std::span<const std::string> workspaceFileUris,
      utils::CancellationToken cancelToken);

  mutable std::mutex _initMutex;
  mutable std::condition_variable _initCv;
  bool _ready = true;
  std::exception_ptr _initializationError;
  std::vector<WorkspaceFolder> _workspaceFolders;
};

} // namespace pegium::workspace
