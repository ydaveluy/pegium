#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <lsp/io/stream.h>
#include <lsp/json/json.h>
#include <lsp/serialization.h>
#include <lsp/types.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/lsp/DefaultLspModule.hpp>
#include <pegium/lsp/DocumentUpdateHandler.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Disposable.hpp>
#include <pegium/workspace/DocumentBuilder.hpp>

namespace pegium::test {

inline std::string with_content_length(std::string_view content) {
  return std::format("Content-Length: {}\r\n\r\n{}", content.size(), content);
}

class MemoryStream final : public ::lsp::io::Stream {
public:
  void pushInput(std::string message) { _input += std::move(message); }

  [[nodiscard]] const std::string &written() const noexcept { return _written; }
  void clearWritten() { _written.clear(); }

  void read(char *buffer, std::size_t size) override {
    for (std::size_t index = 0; index < size; ++index) {
      if (_readOffset < _input.size()) {
        buffer[index] = _input[_readOffset++];
      } else {
        buffer[index] = Eof;
      }
    }
  }

  void write(const char *buffer, std::size_t size) override {
    _written.append(buffer, size);
  }

private:
  std::string _input;
  std::size_t _readOffset = 0;
  std::string _written;
};

template <typename Params>
std::string make_notification_message(std::string_view method, Params params) {
  ::lsp::json::Object message;
  message["jsonrpc"] = std::string("2.0");
  message["method"] = std::string(method);
  message["params"] = ::lsp::toJson(std::move(params));

  const auto content =
      ::lsp::json::stringify(::lsp::json::Value(std::move(message)));
  return with_content_length(content);
}

template <typename Params>
std::string make_request_message(std::int32_t id, std::string_view method,
                                 Params params) {
  ::lsp::json::Object message;
  message["jsonrpc"] = std::string("2.0");
  message["id"] = id;
  message["method"] = std::string(method);
  message["params"] = ::lsp::toJson(std::move(params));

  const auto content =
      ::lsp::json::stringify(::lsp::json::Value(std::move(message)));
  return with_content_length(content);
}

template <typename Result>
std::string make_response_message(std::int32_t id, Result result) {
  ::lsp::json::Object message;
  message["jsonrpc"] = std::string("2.0");
  message["id"] = id;
  message["result"] = ::lsp::toJson(std::move(result));

  const auto content =
      ::lsp::json::stringify(::lsp::json::Value(std::move(message)));
  return with_content_length(content);
}

inline std::string extract_last_message_content(std::string_view written) {
  const auto separator = written.rfind("\r\n\r\n");
  if (separator == std::string_view::npos) {
    return {};
  }
  return std::string(written.substr(separator + 4));
}

inline ::lsp::json::Value parse_last_written_message(std::string_view written) {
  return ::lsp::json::parse(extract_last_message_content(written));
}

inline std::unique_ptr<services::SharedServices> make_shared_services() {
  auto shared =
      std::make_unique<services::SharedServices>(services::SharedServices::NoDefaultsTag{});
  services::installDefaultSharedCoreServices(*shared);
  lsp::installDefaultSharedLspServices(*shared);
  return shared;
}

inline std::unique_ptr<services::Services> make_services(
    const services::SharedServices &sharedServices, std::string languageId,
    std::vector<std::string> fileExtensions = {},
    std::unique_ptr<const parser::Parser> parser =
        std::make_unique<FakeParser>()) {
  auto services =
      services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser = std::move(parser);
  services->languageMetaData.fileExtensions = std::move(fileExtensions);
  return services;
}

template <typename ParserType>
inline std::unique_ptr<services::Services> make_services(
    const services::SharedServices &sharedServices, std::string languageId,
    std::vector<std::string> fileExtensions = {}) {
  auto services =
      services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser = std::make_unique<const ParserType>(*services);
  services->languageMetaData.fileExtensions = std::move(fileExtensions);
  return services;
}

inline std::shared_ptr<workspace::Document>
open_and_build_document(services::SharedServices &sharedServices,
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

class RecordingDocumentBuilder final : public workspace::DocumentBuilder {
public:
  struct BuildCall {
    std::vector<std::string> uris;
    workspace::BuildOptions options;
  };

  struct UpdateCall {
    std::vector<workspace::DocumentId> changedDocumentIds;
    std::vector<workspace::DocumentId> deletedDocumentIds;
    workspace::BuildOptions options;
  };

  [[nodiscard]] workspace::BuildOptions &updateBuildOptions() noexcept override {
    return _options;
  }

  [[nodiscard]] const workspace::BuildOptions &
  updateBuildOptions() const noexcept override {
    return _options;
  }

  [[nodiscard]] bool build(
      std::span<const std::shared_ptr<workspace::Document>> documents,
      const workspace::BuildOptions &options = {},
      utils::CancellationToken cancelToken = {}) const override {
    utils::throw_if_cancelled(cancelToken);

    BuildCall call;
    call.options = options;
    call.uris.reserve(documents.size());
    for (const auto &document : documents) {
      if (document != nullptr) {
        call.uris.push_back(document->uri);
      }
    }
    {
      std::scoped_lock lock(_mutex);
      _buildCalls.push_back(std::move(call));
    }
    _cv.notify_all();
    return true;
  }

  [[nodiscard]] workspace::DocumentUpdateResult
  update(std::span<const workspace::DocumentId> changedDocumentIds,
         std::span<const workspace::DocumentId> deletedDocumentIds,
         utils::CancellationToken cancelToken = {},
         bool rebuildDocuments = true) const override {
    (void)rebuildDocuments;
    utils::throw_if_cancelled(cancelToken);

    UpdateCall call{
        .changedDocumentIds =
            std::vector<workspace::DocumentId>(changedDocumentIds.begin(),
                                              changedDocumentIds.end()),
        .deletedDocumentIds =
            std::vector<workspace::DocumentId>(deletedDocumentIds.begin(),
                                              deletedDocumentIds.end()),
        .options = _options,
    };
    {
      std::scoped_lock lock(_mutex);
      _calls.push_back(call);
    }
    _cv.notify_all();
    return _result;
  }

  utils::ScopedDisposable onUpdate(
      std::function<void(std::span<const workspace::DocumentId>,
                         std::span<const workspace::DocumentId>)>
          listener) override {
    (void)listener;
    return {};
  }

  utils::ScopedDisposable onBuildPhase(
      workspace::DocumentState targetState,
      std::function<void(
          std::span<const std::shared_ptr<workspace::Document>>)> listener) override {
    (void)targetState;
    (void)listener;
    return {};
  }

  utils::ScopedDisposable onDocumentPhase(
      workspace::DocumentState targetState,
      std::function<void(const std::shared_ptr<workspace::Document> &)>
          listener) override {
    (void)targetState;
    (void)listener;
    return {};
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

  bool waitForCalls(std::size_t count,
                    std::chrono::milliseconds timeout =
                        std::chrono::milliseconds(1000)) const {
    std::unique_lock lock(_mutex);
    return _cv.wait_for(lock, timeout,
                        [&]() { return _calls.size() >= count; });
  }

  bool waitForBuildCalls(std::size_t count,
                         std::chrono::milliseconds timeout =
                             std::chrono::milliseconds(1000)) const {
    std::unique_lock lock(_mutex);
    return _cv.wait_for(lock, timeout,
                        [&]() { return _buildCalls.size() >= count; });
  }

  [[nodiscard]] UpdateCall lastCall() const {
    std::scoped_lock lock(_mutex);
    return _calls.empty() ? UpdateCall{} : _calls.back();
  }

  [[nodiscard]] BuildCall lastBuildCall() const {
    std::scoped_lock lock(_mutex);
    return _buildCalls.empty() ? BuildCall{} : _buildCalls.back();
  }

  void setResult(workspace::DocumentUpdateResult result) {
    _result = std::move(result);
  }

private:
  mutable std::mutex _mutex;
  mutable std::condition_variable _cv;
  mutable std::vector<BuildCall> _buildCalls;
  mutable std::vector<UpdateCall> _calls;
  workspace::BuildOptions _options{};
  workspace::DocumentUpdateResult _result{};
};

class RecordingDocumentUpdateHandler final : public lsp::DocumentUpdateHandler {
public:
  void didChangeWatchedFiles(
      const ::lsp::DidChangeWatchedFilesParams &params) override {
    std::scoped_lock lock(_mutex);
    _calls.push_back(params);
  }

  [[nodiscard]] std::size_t callCount() const {
    std::scoped_lock lock(_mutex);
    return _calls.size();
  }

  [[nodiscard]] ::lsp::DidChangeWatchedFilesParams lastCall() const {
    std::scoped_lock lock(_mutex);
    return _calls.empty() ? ::lsp::DidChangeWatchedFilesParams{} : _calls.back();
  }

private:
  mutable std::mutex _mutex;
  std::vector<::lsp::DidChangeWatchedFilesParams> _calls;
};

} // namespace pegium::test
