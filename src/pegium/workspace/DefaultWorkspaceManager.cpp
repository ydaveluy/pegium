#include <pegium/workspace/DefaultWorkspaceManager.hpp>

#include <cassert>
#include <cctype>
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <optional>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>

#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/utils/UriUtils.hpp>
#include <pegium/validation/ValidationRegistry.hpp>
#include <pegium/workspace/DefaultWorkspaceLock.hpp>

namespace pegium::workspace {

namespace {

Documents &documents(services::SharedCoreServices &sharedServices) {
  assert(sharedServices.workspace.documents != nullptr);
  return *sharedServices.workspace.documents;
}

const Documents &documents(const services::SharedCoreServices &sharedServices) {
  assert(sharedServices.workspace.documents != nullptr);
  return *sharedServices.workspace.documents;
}

DocumentBuilder &builder(services::SharedCoreServices &sharedServices) {
  assert(sharedServices.workspace.documentBuilder != nullptr);
  return *sharedServices.workspace.documentBuilder;
}

void collect_workspace_files(const FileSystemProvider &fileSystem,
                            std::string_view workspaceUri,
                            std::string_view entryUri,
                            const std::function<bool(
                                std::string_view, std::string_view, bool)>
                                &shouldIncludeEntry,
                            std::vector<std::string> &uris) {
  if (!fileSystem.exists(entryUri)) {
    return;
  }

  const auto entry = fileSystem.stat(entryUri);
  if (!shouldIncludeEntry(workspaceUri, entry.uri, entry.isDirectory)) {
    return;
  }

  if (!entry.isDirectory) {
    uris.push_back(entry.uri);
    return;
  }

  for (const auto &child : fileSystem.readDirectory(entry.uri)) {
    collect_workspace_files(fileSystem, workspaceUri, child.uri,
                            shouldIncludeEntry, uris);
  }
}

std::string normalize_pattern(std::string_view pattern) {
  while (!pattern.empty() &&
         std::isspace(static_cast<unsigned char>(pattern.front())) != 0) {
    pattern.remove_prefix(1);
  }
  while (!pattern.empty() &&
         std::isspace(static_cast<unsigned char>(pattern.back())) != 0) {
    pattern.remove_suffix(1);
  }

  std::string normalized(pattern);
  for (auto &ch : normalized) {
    if (ch == '\\') {
      ch = '/';
    }
  }
  while (normalized.starts_with("./")) {
    normalized.erase(0, 2);
  }
  while (normalized.size() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  return normalized;
}

std::string normalize_path(std::string_view path) {
  std::string normalized(path);
  for (auto &ch : normalized) {
    if (ch == '\\') {
      ch = '/';
    }
  }
  while (normalized.starts_with("./")) {
    normalized.erase(0, 2);
  }
  while (normalized.size() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  return normalized;
}

std::string relative_workspace_path(std::string_view workspaceUri,
                                    std::string_view entryUri) {
  if (utils::equals_uri(workspaceUri, entryUri)) {
    return {};
  }

  auto relative = normalize_path(utils::relative_uri(workspaceUri, entryUri));
  while (relative.starts_with("/")) {
    relative.erase(relative.begin());
  }
  return relative;
}

std::string glob_to_regex(std::string_view pattern) {
  auto normalized = normalize_pattern(pattern);
  if (normalized.empty()) {
    return {};
  }

  const bool anchoredToRoot = normalized.front() == '/';
  if (anchoredToRoot) {
    normalized.erase(normalized.begin());
  }

  bool matchesDescendants = false;
  if (normalized.size() >= 3 && normalized.ends_with("/**")) {
    normalized.resize(normalized.size() - 3);
    matchesDescendants = true;
  }

  std::string regex = anchoredToRoot ? "^" : "(^|.*/)";
  for (std::size_t index = 0; index < normalized.size(); ++index) {
    const auto ch = normalized[index];
    if (ch == '*') {
      if (index + 1 < normalized.size() && normalized[index + 1] == '*') {
        if (index + 2 < normalized.size() && normalized[index + 2] == '/') {
          regex += "(?:.*/)?";
          index += 2;
        } else {
          regex += ".*";
          ++index;
        }
        continue;
      }
      regex += "[^/]*";
      continue;
    }
    if (ch == '?') {
      regex += "[^/]";
      continue;
    }
    if (ch == '.' || ch == '^' || ch == '$' || ch == '|' || ch == '(' ||
        ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
        ch == '+' || ch == '\\') {
      regex.push_back('\\');
    }
    regex.push_back(ch);
  }

  if (matchesDescendants) {
    regex += "(?:/.*)?";
  }
  regex += "$";
  return regex;
}

class BuildIgnoreMatcher {
public:
  explicit BuildIgnoreMatcher(std::vector<std::regex> patterns = {})
      : _patterns(std::move(patterns)) {}

  [[nodiscard]] bool ignores(std::string_view relativePath) const {
    const auto normalized = normalize_path(relativePath);
    if (normalized.empty()) {
      return false;
    }
    for (const auto &pattern : _patterns) {
      if (std::regex_match(normalized, pattern)) {
        return true;
      }
    }
    return false;
  }

private:
  std::vector<std::regex> _patterns;
};

BuildIgnoreMatcher build_ignore_matcher(
    const ConfigurationProvider *configurationProvider) {
  if (configurationProvider == nullptr) {
    return BuildIgnoreMatcher{};
  }

  const auto buildConfiguration =
      configurationProvider->getConfiguration("pegium", "build");
  if (!buildConfiguration.has_value() || !buildConfiguration->isObject()) {
    return BuildIgnoreMatcher{};
  }

  const auto ignorePatternsIt =
      buildConfiguration->object().find("ignorePatterns");
  if (ignorePatternsIt == buildConfiguration->object().end()) {
    return BuildIgnoreMatcher{};
  }

  std::vector<std::regex> patterns;
  const auto addPattern = [&patterns](std::string_view pattern) {
    const auto regex = glob_to_regex(pattern);
    if (!regex.empty()) {
      patterns.emplace_back(regex, std::regex::ECMAScript);
    }
  };

  const auto &ignorePatterns = ignorePatternsIt->second;
  if (ignorePatterns.isString()) {
    std::string_view remaining = ignorePatterns.string();
    while (true) {
      const auto separator = remaining.find(',');
      addPattern(remaining.substr(0, separator));
      if (separator == std::string_view::npos) {
        break;
      }
      remaining.remove_prefix(separator + 1);
    }
  } else if (ignorePatterns.isArray()) {
    for (const auto &pattern : ignorePatterns.array()) {
      if (pattern.isString()) {
        addPattern(pattern.string());
      }
    }
  }

  return BuildIgnoreMatcher(std::move(patterns));
}

bool should_include_entry(const services::SharedCoreServices &sharedServices,
                          std::string_view workspaceUri,
                          std::string_view uri, bool isDirectory,
                          const BuildIgnoreMatcher &ignoreMatcher) {
  if (ignoreMatcher.ignores(relative_workspace_path(workspaceUri, uri))) {
    return false;
  }

  const auto path = utils::file_uri_to_path(uri).value_or(std::string(uri));
  const auto fileName =
      std::filesystem::path(std::move(path)).filename().string();
  if (fileName.empty()) {
    return false;
  }
  if (!fileName.empty() && fileName.front() == '.') {
    return false;
  }

  if (isDirectory) {
    return fileName != "node_modules" && fileName != "out";
  }

  if (sharedServices.serviceRegistry == nullptr) {
    return false;
  }
  return sharedServices.serviceRegistry->hasServices(uri);
}

} // namespace

DefaultWorkspaceManager::DefaultWorkspaceManager(
    services::SharedCoreServices &sharedServices)
    : services::DefaultSharedCoreService(sharedServices) {
  _initialBuildOptions.validation.enabled = true;
  _initialBuildOptions.validation.categories = {
      std::string(validation::kBuiltInValidationCategory),
      std::string(validation::kFastValidationCategory)};
  if (!sharedCoreServices.workspace.configurationProvider) {
    sharedCoreServices.workspace.configurationProvider =
        make_default_configuration_provider(
            sharedCoreServices.serviceRegistry.get());
  }
  if (!sharedCoreServices.workspace.fileSystemProvider) {
    sharedCoreServices.workspace.fileSystemProvider =
        std::make_shared<LocalFileSystemProvider>();
  }
  if (!sharedCoreServices.workspace.workspaceLock) {
    sharedCoreServices.workspace.workspaceLock =
        std::make_unique<DefaultWorkspaceLock>();
  }
}

BuildOptions &DefaultWorkspaceManager::initialBuildOptions() {
  return _initialBuildOptions;
}

const BuildOptions &DefaultWorkspaceManager::initialBuildOptions() const {
  return _initialBuildOptions;
}

bool DefaultWorkspaceManager::isReady() const noexcept {
  std::scoped_lock lock(_initMutex);
  return _ready;
}

void DefaultWorkspaceManager::waitUntilReady(
    utils::CancellationToken cancelToken) const {
  if (cancelToken.stop_requested()) {
    throw utils::OperationCancelled();
  }

  std::unique_lock lock(_initMutex);
  constexpr auto kWaitStep = std::chrono::milliseconds(10);
  while (!_ready) {
    if (cancelToken.stop_requested()) {
      throw utils::OperationCancelled();
    }
    _initCv.wait_for(lock, kWaitStep);
  }
  if (_initializationError != nullptr) {
    std::rethrow_exception(_initializationError);
  }
}

std::vector<WorkspaceFolder> DefaultWorkspaceManager::workspaceFolders() const {
  std::scoped_lock lock(_initMutex);
  return _workspaceFolders;
}

void DefaultWorkspaceManager::initialize(const InitializeParams &params) {
  std::scoped_lock lock(_initMutex);
  _workspaceFolders.clear();
  _ready = false;
  _initializationError = nullptr;
  _workspaceFolders = params.workspaceFolders;
  _initCv.notify_all();
}

std::future<void> DefaultWorkspaceManager::initialized(
    const InitializedParams &params,
    utils::CancellationToken cancelToken) {
  std::vector<WorkspaceFolder> folders;
  {
    std::scoped_lock lock(_initMutex);
    folders = _workspaceFolders;
  }

  return std::async(std::launch::async,
                    [this, params, folders = std::move(folders), cancelToken]() {
                      (void)params;
                      std::exception_ptr error;
                      try {
                        auto future = initializeWorkspace(folders, cancelToken);
                        if (future.valid()) {
                          future.get();
                        }
                      } catch (...) {
                        error = std::current_exception();
                      }
                      {
                        std::scoped_lock lock(_initMutex);
                        _initializationError = error;
                        _ready = true;
                      }
                      _initCv.notify_all();
                      if (error != nullptr) {
                        std::rethrow_exception(error);
                      }
                    });
}

std::future<void> DefaultWorkspaceManager::initializeWorkspace(
    std::span<const WorkspaceFolder> workspaceFolders,
    utils::CancellationToken cancelToken) {
  return std::async(std::launch::async,
                    [this,
                     folders = std::vector<WorkspaceFolder>(
                         workspaceFolders.begin(), workspaceFolders.end()),
                     cancelToken]() {
                      utils::throw_if_cancelled(cancelToken);
                      std::unordered_set<std::string> seenUris;
                      std::vector<std::string> uniqueFileUris;
                      uniqueFileUris.reserve(64);

                      for (const auto &workspaceFolder : folders) {
                        utils::throw_if_cancelled(cancelToken);
                        const auto workspaceUri = getRootFolder(workspaceFolder);
                        if (workspaceUri.empty()) {
                          continue;
                        }

                        for (const auto &fileUri : searchFolder(workspaceUri)) {
                          if (!seenUris.insert(fileUri).second) {
                            continue;
                          }
                          uniqueFileUris.push_back(fileUri);
                        }
                      }

                      run_with_workspace_write(
                          sharedCoreServices.workspace.workspaceLock.get(),
                          cancelToken, [&]() {
                            auto loadedDocuments = performStartup(
                                folders, uniqueFileUris, cancelToken);
                            utils::throw_if_cancelled(cancelToken);
                            if (!loadedDocuments.empty()) {
                              (void)builder(sharedCoreServices).build(
                                  loadedDocuments, _initialBuildOptions,
                                  cancelToken);
                            }
                          });
                    });
}

std::string
DefaultWorkspaceManager::getRootFolder(
    const WorkspaceFolder &workspaceFolder) const {
  return workspaceFolder.uri;
}

void DefaultWorkspaceManager::loadAdditionalDocuments(
    std::span<const WorkspaceFolder> workspaceFolders,
    const std::function<void(std::shared_ptr<Document>)> &collector,
    utils::CancellationToken cancelToken) {
  (void)workspaceFolders;
  (void)collector;
  utils::throw_if_cancelled(cancelToken);
}

void DefaultWorkspaceManager::loadWorkspaceDocuments(
    std::span<const std::string> workspaceFileUris,
    const std::function<void(std::shared_ptr<Document>)> &collector,
    utils::CancellationToken cancelToken) {
  for (const auto &fileUri : workspaceFileUris) {
    utils::throw_if_cancelled(cancelToken);
    if (documents(sharedCoreServices).getDocument(fileUri) != nullptr) {
      continue;
    }
    auto document = documents(sharedCoreServices)
                        .getOrCreateDocument(fileUri, cancelToken);
    if (document != nullptr && collector) {
      collector(std::move(document));
    }
  }
}

std::vector<std::shared_ptr<Document>> DefaultWorkspaceManager::performStartup(
    std::span<const WorkspaceFolder> workspaceFolders,
    std::span<const std::string> workspaceFileUris,
    utils::CancellationToken cancelToken) {
  std::vector<std::shared_ptr<Document>> loadedDocuments;
  loadedDocuments.reserve(workspaceFileUris.size());

  auto collector = [this, &loadedDocuments](std::shared_ptr<Document> document) {
    if (document == nullptr || document->uri.empty()) {
      return;
    }
    if (!documents(sharedCoreServices).hasDocument(document->uri)) {
      documents(sharedCoreServices).addDocument(document);
    }
    loadedDocuments.push_back(std::move(document));
  };

  loadAdditionalDocuments(workspaceFolders, collector, cancelToken);
  loadWorkspaceDocuments(workspaceFileUris, collector, cancelToken);
  return loadedDocuments;
}

std::vector<std::string>
DefaultWorkspaceManager::searchFolder(std::string_view workspaceUri) const {
  return run_with_workspace_read(sharedCoreServices.workspace.workspaceLock.get(),
                                 [&]() {
    if (sharedCoreServices.workspace.fileSystemProvider == nullptr) {
      return std::vector<std::string>{};
    }

    const auto ignoreMatcher =
        build_ignore_matcher(
            sharedCoreServices.workspace.configurationProvider.get());
    std::vector<std::string> uris;
    const auto shouldIncludeEntry =
        [this, &ignoreMatcher](std::string_view currentWorkspaceUri,
                               std::string_view entryUri, bool isDirectory) {
          return should_include_entry(sharedCoreServices, currentWorkspaceUri,
                                      entryUri, isDirectory, ignoreMatcher);
        };
    collect_workspace_files(*sharedCoreServices.workspace.fileSystemProvider,
                            workspaceUri, utils::normalize_uri(workspaceUri),
                            shouldIncludeEntry, uris);
    return uris;
  });
}

bool DefaultWorkspaceManager::shouldIncludeEntry(std::string_view workspaceUri,
                                                 std::string_view uri,
                                                 bool isDirectory) const {
  const auto ignoreMatcher =
      build_ignore_matcher(sharedCoreServices.workspace.configurationProvider.get());
  return should_include_entry(sharedCoreServices, workspaceUri, uri,
                              isDirectory, ignoreMatcher);
}

} // namespace pegium::workspace
