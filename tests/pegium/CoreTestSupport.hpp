#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <thread>

#include <pegium/parser/Parser.hpp>
#include <pegium/services/CoreServices.hpp>
#include <pegium/services/DefaultServiceRegistry.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/utils/UriUtils.hpp>
#include <pegium/validation/DocumentValidator.hpp>
#include <pegium/workspace/DefaultDocumentFactory.hpp>
#include <pegium/workspace/DefaultDocuments.hpp>
#include <pegium/workspace/DefaultTextDocuments.hpp>
#include <pegium/workspace/DefaultWorkspaceLock.hpp>
#include <pegium/workspace/DocumentFactory.hpp>
#include <pegium/workspace/FileSystemProvider.hpp>

namespace pegium::test {

inline std::string make_file_uri(std::string_view fileName) {
  return utils::path_to_file_uri("/tmp/pegium-tests/" + std::string(fileName));
}

inline bool wait_until(std::function<bool()> predicate,
                       std::chrono::milliseconds timeout =
                           std::chrono::milliseconds(1000),
                       std::chrono::milliseconds step =
                           std::chrono::milliseconds(10)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(step);
  }
  return predicate();
}

class FakeParser final : public parser::Parser {
public:
  using ParseCallback = std::function<void(workspace::Document &)>;

  bool fullMatch = true;
  std::vector<parser::ParseDiagnostic> parseDiagnostics;
  parser::ExpectResult expectations;
  ParseCallback callback;
  mutable std::size_t parseCalls = 0;
  mutable std::vector<std::string> parsedTexts;
  mutable std::mutex mutex;

  void parse(workspace::Document &document,
             const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    {
      std::scoped_lock lock(mutex);
      ++parseCalls;
      parsedTexts.push_back(document.text());
    }
    document.parseResult = {};
    document.parseResult.fullMatch = fullMatch;
    document.parseResult.parsedLength =
        static_cast<TextOffset>(document.text().size());
    document.parseResult.maxCursorOffset = document.parseResult.parsedLength;
    document.parseResult.parseDiagnostics = parseDiagnostics;
    if (callback) {
      callback(document);
    }
  }

  [[nodiscard]] parser::ExpectResult expect(
      std::string_view, TextOffset,
      const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    return expectations;
  }
};

class FakeFileSystemProvider final : public workspace::FileSystemProvider {
public:
  std::unordered_map<std::string, std::string> files;
  std::unordered_map<std::string, std::vector<std::string>> directories;

  [[nodiscard]] workspace::FileSystemNode
  stat(std::string_view uri) const override {
    const auto key = normalizeKey(uri);
    return workspace::FileSystemNode{
        .isFile = files.contains(key),
        .isDirectory = directories.contains(key),
        .uri = toFileUri(key)};
  }

  [[nodiscard]] bool exists(std::string_view uri) const override {
    const auto key = normalizeKey(uri);
    return files.contains(key) || directories.contains(key);
  }

  [[nodiscard]] std::vector<std::uint8_t>
  readBinary(std::string_view uri) const override {
    const auto content = readFile(uri);
    if (!content.has_value()) {
      return {};
    }
    return std::vector<std::uint8_t>(content->begin(), content->end());
  }

  [[nodiscard]] std::optional<std::string>
  readFile(std::string_view uri) const override {
    const auto it = files.find(normalizeKey(uri));
    if (it == files.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  [[nodiscard]] std::vector<workspace::FileSystemNode>
  readDirectory(std::string_view uri) const override {
    const auto it = directories.find(normalizeKey(uri));
    if (it == directories.end()) {
      return {};
    }
    std::vector<workspace::FileSystemNode> nodes;
    nodes.reserve(it->second.size());
    for (const auto &child : it->second) {
      nodes.push_back(stat(toFileUri(child)));
    }
    return nodes;
  }

private:
  [[nodiscard]] static std::string normalizeKey(std::string_view uriOrPath) {
    return utils::file_uri_to_path(uriOrPath).value_or(std::string(uriOrPath));
  }

  [[nodiscard]] static std::string toFileUri(std::string_view uriOrPath) {
    if (utils::is_file_uri(uriOrPath)) {
      return std::string(uriOrPath);
    }
    return utils::path_to_file_uri(uriOrPath);
  }
};

class FakeDocumentValidator final : public validation::DocumentValidator {
public:
  std::vector<services::Diagnostic> diagnostics;
  mutable std::size_t validateCalls = 0;
  mutable std::vector<validation::ValidationOptions> seenOptions;
  mutable std::vector<services::Diagnostic> diagnosticsByCall;
  mutable std::size_t diagnosticIndex = 0;
  mutable std::mutex mutex;

  [[nodiscard]] std::vector<services::Diagnostic>
  validateDocument(const workspace::Document &document,
                   const validation::ValidationOptions &options) const override {
    (void)document;
    std::scoped_lock lock(mutex);
    ++validateCalls;
    seenOptions.push_back(options);
    if (!diagnosticsByCall.empty()) {
      const auto index = std::min(diagnosticIndex, diagnosticsByCall.size() - 1);
      ++diagnosticIndex;
      return {diagnosticsByCall[index]};
    }
    return diagnostics;
  }
};

class FakeDocumentFactory final : public workspace::DocumentFactory {
public:
  std::unordered_map<std::string, std::string> contentsByUri;
  std::vector<std::string> fromUriCalls;
  std::vector<std::string> fromStringCalls;
  std::size_t updateCalls = 0;
  mutable std::mutex mutex;

  [[nodiscard]] std::shared_ptr<workspace::Document> fromTextDocument(
      std::shared_ptr<const workspace::TextDocument> textDocument,
      const utils::CancellationToken &cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    if (textDocument == nullptr) {
      return nullptr;
    }
    auto document = std::make_shared<workspace::Document>();
    document->setTextDocument(std::move(textDocument));
    document->state = workspace::DocumentState::Parsed;
    return document;
  }

  [[nodiscard]] std::shared_ptr<workspace::Document>
  fromString(std::string text, std::string uri, std::string languageId = {},
             std::optional<std::int64_t> clientVersion = std::nullopt,
             const utils::CancellationToken &cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    auto textDocument = std::make_shared<workspace::TextDocument>();
    textDocument->uri = std::move(uri);
    textDocument->languageId = std::move(languageId);
    textDocument->replaceText(std::move(text));
    textDocument->setClientVersion(clientVersion);

    auto document = std::make_shared<workspace::Document>();
    document->setTextDocument(std::move(textDocument));
    document->state = workspace::DocumentState::Parsed;
    return document;
  }

  [[nodiscard]] std::shared_ptr<workspace::Document>
  fromUri(std::string_view uri,
          const utils::CancellationToken &cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    const auto it = contentsByUri.find(std::string(uri));
    if (it == contentsByUri.end()) {
      return nullptr;
    }
    auto textDocument = std::make_shared<workspace::TextDocument>();
    textDocument->uri = std::string(uri);
    textDocument->replaceText(it->second);
    auto document = std::make_shared<workspace::Document>();
    document->setTextDocument(std::move(textDocument));
    document->state = workspace::DocumentState::Parsed;
    return document;
  }

  workspace::Document &
  update(workspace::Document &document,
         const utils::CancellationToken &cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    {
      std::scoped_lock lock(mutex);
      ++const_cast<FakeDocumentFactory *>(this)->updateCalls;
    }
    if (document.state < workspace::DocumentState::Parsed) {
      document.state = workspace::DocumentState::Parsed;
    }
    return document;
  }
};

class RecordingEventDocumentBuilder final : public workspace::DocumentBuilder {
public:
  [[nodiscard]] workspace::BuildOptions &updateBuildOptions() noexcept override {
    return _options;
  }

  [[nodiscard]] const workspace::BuildOptions &
  updateBuildOptions() const noexcept override {
    return _options;
  }

  [[nodiscard]] bool build(
      std::span<const std::shared_ptr<workspace::Document>>,
      const workspace::BuildOptions & = {},
      utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);
    return true;
  }

  [[nodiscard]] workspace::DocumentUpdateResult
  update(std::span<const workspace::DocumentId> changedDocumentIds,
         std::span<const workspace::DocumentId> deletedDocumentIds,
         utils::CancellationToken cancelToken = {},
         bool rebuildDocuments = true) const override {
    (void)rebuildDocuments;
    (void)changedDocumentIds;
    utils::throw_if_cancelled(cancelToken);
    workspace::DocumentUpdateResult result;
    result.rebuiltDocuments = _updatedDocuments;
    result.deletedDocumentIds.assign(deletedDocumentIds.begin(),
                                     deletedDocumentIds.end());
    emitUpdate({}, {});
    return result;
  }

  utils::ScopedDisposable onUpdate(
      std::function<void(std::span<const workspace::DocumentId>,
                         std::span<const workspace::DocumentId>)>
          listener) override {
    return _onUpdate.on([listener = std::move(listener)](
                            const workspace::DocumentUpdateEvent &event) {
      if (listener) {
        listener(event.changedDocumentIds, event.deletedDocumentIds);
      }
    });
  }

  utils::ScopedDisposable onBuildPhase(
      workspace::DocumentState targetState,
      std::function<void(
          std::span<const std::shared_ptr<workspace::Document>>)> listener) override {
    return _onBuildPhase.on([targetState, listener = std::move(listener)](
                                const workspace::DocumentBuildPhaseEvent &event) {
      if (listener && event.targetState == targetState) {
        listener(event.builtDocuments);
      }
    });
  }

  utils::ScopedDisposable onDocumentPhase(
      workspace::DocumentState targetState,
      std::function<void(const std::shared_ptr<workspace::Document> &)>
          listener) override {
    return _onDocumentPhase.on(
        [targetState, listener = std::move(listener)](
            const workspace::DocumentPhaseEvent &event) {
          if (listener && event.targetState == targetState &&
              event.builtDocument != nullptr) {
            listener(event.builtDocument);
          }
        });
  }

  void waitUntil(workspace::DocumentState state,
                 utils::CancellationToken cancelToken = {}) const override {
    (void)state;
    utils::throw_if_cancelled(cancelToken);
  }

  void waitUntil(workspace::DocumentState state, workspace::DocumentId documentId,
                 utils::CancellationToken cancelToken = {}) const override {
    (void)state;
    (void)documentId;
    utils::throw_if_cancelled(cancelToken);
  }

  void resetToState(workspace::Document &document,
                    workspace::DocumentState state) const override {
    document.state = state;
  }

  void emitUpdate(std::vector<workspace::DocumentId> changedDocumentIds,
                  std::vector<workspace::DocumentId> deletedDocumentIds) const {
    _onUpdate.emit({.changedDocumentIds = std::move(changedDocumentIds),
                    .deletedDocumentIds = std::move(deletedDocumentIds)});
  }

  void emitBuildPhase(
      workspace::DocumentState state,
      std::vector<std::shared_ptr<workspace::Document>> documents = {}) const {
    _onBuildPhase.emit(
        {.targetState = state, .builtDocuments = std::move(documents)});
  }

  void emitDocumentPhase(
      workspace::DocumentState state,
      std::shared_ptr<workspace::Document> document) const {
    _onDocumentPhase.emit(
        {.targetState = state, .builtDocument = std::move(document)});
  }

  [[nodiscard]] std::size_t updateListenerCount() const {
    return _onUpdate.listenerCount();
  }

  [[nodiscard]] std::size_t buildPhaseListenerCount() const {
    return _onBuildPhase.listenerCount();
  }

  [[nodiscard]] std::size_t documentPhaseListenerCount() const {
    return _onDocumentPhase.listenerCount();
  }

  void setUpdatedDocuments(
      std::vector<std::shared_ptr<workspace::Document>> documents) {
    _updatedDocuments = std::move(documents);
  }

private:
  workspace::BuildOptions _options{};
  mutable utils::EventEmitter<workspace::DocumentUpdateEvent> _onUpdate;
  mutable utils::EventEmitter<workspace::DocumentBuildPhaseEvent> _onBuildPhase;
  mutable utils::EventEmitter<workspace::DocumentPhaseEvent> _onDocumentPhase;
  std::vector<std::shared_ptr<workspace::Document>> _updatedDocuments;
};

struct TestCoreServices final : services::CoreServices {
  explicit TestCoreServices(const services::SharedCoreServices &sharedServices)
      : services::CoreServices(sharedServices) {}
};

inline std::unique_ptr<services::SharedCoreServices> make_shared_core_services() {
  auto shared = std::make_unique<services::SharedCoreServices>();
  services::installDefaultSharedCoreServices(*shared);
  return shared;
}

inline std::unique_ptr<TestCoreServices> make_core_services(
    const services::SharedCoreServices &sharedServices, std::string languageId,
    std::vector<std::string> fileExtensions = {},
    std::vector<std::string> fileNames = {},
    std::unique_ptr<const parser::Parser> parser =
        std::make_unique<FakeParser>()) {
  auto services = std::make_unique<TestCoreServices>(sharedServices);
  services->languageId = std::move(languageId);
  services->languageMetaData.languageId = services->languageId;
  services->languageMetaData.fileExtensions = std::move(fileExtensions);
  services->languageMetaData.fileNames = std::move(fileNames);
  services->parser = std::move(parser);
  installDefaultCoreServices(*services);
  return services;
}

template <typename ParserType>
inline std::unique_ptr<TestCoreServices> make_core_services(
    const services::SharedCoreServices &sharedServices, std::string languageId,
    std::vector<std::string> fileExtensions = {},
    std::vector<std::string> fileNames = {}) {
  auto services = std::make_unique<TestCoreServices>(sharedServices);
  services->languageId = std::move(languageId);
  services->languageMetaData.languageId = services->languageId;
  services->languageMetaData.fileExtensions = std::move(fileExtensions);
  services->languageMetaData.fileNames = std::move(fileNames);
  installDefaultCoreServices(*services);
  services->parser = std::make_unique<const ParserType>(*services);
  return services;
}

inline std::shared_ptr<workspace::Document>
open_and_build_document(services::SharedCoreServices &sharedServices,
                        std::string uri, std::string languageId,
                        std::string text) {
  auto textDocument = sharedServices.workspace.textDocuments->open(
      std::move(uri), std::move(languageId), std::move(text), 1);
  const auto documentId =
      sharedServices.workspace.documents->getOrCreateDocumentId(textDocument->uri);
  const std::array<workspace::DocumentId, 1> changedDocumentIds{documentId};
  (void)sharedServices.workspace.documentBuilder->update(changedDocumentIds, {});
  return sharedServices.workspace.documents->getDocument(textDocument->uri);
}

} // namespace pegium::test
