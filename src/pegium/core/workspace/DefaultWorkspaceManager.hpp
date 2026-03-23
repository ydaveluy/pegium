#pragma once

#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <pegium/core/services/DefaultSharedCoreService.hpp>
#include <pegium/core/utils/FunctionRef.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/WorkspaceManager.hpp>

namespace pegium::workspace {

/// Default workspace manager handling startup discovery and initial builds.
class DefaultWorkspaceManager : public WorkspaceManager,
                                protected services::DefaultSharedCoreService {
public:
  explicit DefaultWorkspaceManager(const services::SharedCoreServices &sharedServices);

  [[nodiscard]] BuildOptions &initialBuildOptions() override;
  [[nodiscard]] const BuildOptions &initialBuildOptions() const override;
  [[nodiscard]] std::shared_future<void> ready() const override;
  [[nodiscard]] std::optional<std::vector<WorkspaceFolder>>
  workspaceFolders() const override;

  void initialize(const InitializeParams &params) override;
  [[nodiscard]] std::future<void>
  initialized(const InitializedParams &params) override;
  void initializeWorkspace(
      std::span<const WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken = {}) override;
  [[nodiscard]] std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const override;
  [[nodiscard]] bool shouldIncludeEntry(const FileSystemNode &entry) const override;

protected:
  [[nodiscard]] virtual std::string
  getRootFolder(const WorkspaceFolder &workspaceFolder) const;

  /// `collector` must only receive non-null managed documents with a
  /// normalized non-empty URI.
  virtual void loadAdditionalDocuments(
      std::span<const WorkspaceFolder> workspaceFolders,
      utils::function_ref<void(std::shared_ptr<Document>)> collector,
      utils::CancellationToken cancelToken);

  /// `collector` must only receive non-null managed documents with a
  /// normalized non-empty URI.
  virtual void loadWorkspaceDocuments(
      std::span<const std::string> workspaceFileUris,
      utils::function_ref<void(std::shared_ptr<Document>)> collector,
      utils::CancellationToken cancelToken);

  BuildOptions _initialBuildOptions;

private:
  [[nodiscard]] std::vector<std::shared_ptr<Document>> performStartup(
      std::span<const WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken);
  void traverseFolder(const FileSystemProvider &fileSystem,
                      std::string_view folderUri,
                      std::vector<std::string> &workspaceFileUris,
                      utils::CancellationToken cancelToken) const;
  void resolveReady();
  void rejectReady(std::exception_ptr error);

  mutable std::mutex _initMutex;
  std::shared_ptr<std::promise<void>> _readyPromise;
  std::shared_future<void> _readyFuture;
  bool _readySettled = false;
  std::optional<std::vector<WorkspaceFolder>> _workspaceFolders;
};

} // namespace pegium::workspace
