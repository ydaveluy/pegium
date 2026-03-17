#pragma once

#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/workspace/FileSystemProvider.hpp>
#include <pegium/workspace/WorkspaceProtocol.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::services {
class SharedServices;
}

namespace pegium::workspace {

struct BuildOptions;

class WorkspaceManager {
public:
  virtual ~WorkspaceManager() noexcept = default;

  [[nodiscard]] virtual BuildOptions &initialBuildOptions() = 0;
  [[nodiscard]] virtual const BuildOptions &initialBuildOptions() const = 0;
  [[nodiscard]] virtual bool isReady() const noexcept = 0;
  virtual void
  waitUntilReady(utils::CancellationToken cancelToken = {}) const = 0;
  [[nodiscard]] virtual std::vector<WorkspaceFolder> workspaceFolders() const = 0;

  virtual void initialize(const InitializeParams &params) = 0;
  [[nodiscard]] virtual std::future<void>
  initialized(const InitializedParams &params,
              utils::CancellationToken cancelToken = {}) = 0;
  [[nodiscard]] virtual std::future<void>
  initializeWorkspace(std::span<const WorkspaceFolder> workspaceFolders,
                      utils::CancellationToken cancelToken = {}) = 0;
  [[nodiscard]] virtual std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const = 0;
  [[nodiscard]] virtual bool
  shouldIncludeEntry(std::string_view workspaceUri, std::string_view path,
                     bool isDirectory) const = 0;
};

} // namespace pegium::workspace
