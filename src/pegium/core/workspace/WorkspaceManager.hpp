#pragma once

#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/core/workspace/FileSystemProvider.hpp>
#include <pegium/core/workspace/WorkspaceProtocol.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium {
class SharedServices;
}

namespace pegium::workspace {

struct BuildOptions;

/// Owns workspace discovery, startup loading, and initial build orchestration.
class WorkspaceManager {
public:
  virtual ~WorkspaceManager() noexcept = default;

  /// Returns the build options used for the initial workspace build.
  [[nodiscard]] virtual BuildOptions &initialBuildOptions() = 0;
  /// Returns the build options used for the initial workspace build.
  [[nodiscard]] virtual const BuildOptions &initialBuildOptions() const = 0;
  /// Resolves once startup documents have been discovered and materialized.
  ///
  /// This readiness signal does not guarantee that the initial document build
  /// has finished or that documents reached any particular analysis state.
  /// Callers that require stable parse, linking, or validation results should
  /// wait on `DocumentBuilder::waitUntil(...)` instead.
  [[nodiscard]] virtual std::shared_future<void> ready() const = 0;
  /// Returns the workspace folders known after initialization, when available.
  [[nodiscard]] virtual std::optional<std::vector<WorkspaceFolder>>
  workspaceFolders() const = 0;

  /// Records initialize-time workspace metadata and capabilities.
  virtual void initialize(const InitializeParams &params) = 0;
  /// Starts the initial workspace discovery and build under the workspace lock.
  ///
  /// A newer write may supersede the tail of this bootstrap work through the
  /// workspace lock cancellation contract, so completion of this future is not
  /// the only way startup documents can become available.
  [[nodiscard]] virtual std::future<void>
  initialized(const InitializedParams &params) = 0;
  /// Loads startup documents under the workspace lock and starts the initial build.
  virtual void initializeWorkspace(
      std::span<const WorkspaceFolder> workspaceFolders,
      utils::CancellationToken cancelToken = {}) = 0;
  /// Returns the file URIs currently discoverable under one workspace folder.
  [[nodiscard]] virtual std::vector<std::string>
  searchFolder(std::string_view workspaceUri) const = 0;
  /// Returns whether one file-system entry should participate in workspace discovery.
  [[nodiscard]] virtual bool
  shouldIncludeEntry(const FileSystemNode &entry) const = 0;
};

} // namespace pegium::workspace
