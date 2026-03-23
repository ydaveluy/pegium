#include <pegium/lsp/workspace/DefaultFileOperationHandler.hpp>

#include <cassert>
#include <utility>

#include <pegium/lsp/workspace/DocumentUpdateHandler.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

namespace {

void forward_file_changes(const pegium::SharedServices &sharedServices,
                          ::lsp::Array<::lsp::FileEvent> changes) {
  if (changes.empty()) {
    return;
  }
  assert(sharedServices.lsp.documentUpdateHandler != nullptr);

  ::lsp::DidChangeWatchedFilesParams params{};
  params.changes = std::move(changes);
  sharedServices.lsp.documentUpdateHandler->didChangeWatchedFiles(params);
}

} // namespace

DefaultFileOperationHandler::DefaultFileOperationHandler(
    pegium::SharedServices &sharedServices)
    : DefaultSharedLspService(sharedServices) {
  ::lsp::FileOperationFilter fileFilter{};
  fileFilter.pattern.glob = "**/*";
  fileFilter.scheme = "file";

  ::lsp::FileOperationRegistrationOptions fileRegistration{};
  fileRegistration.filters.push_back(std::move(fileFilter));

  _fileOperationOptions.didCreate = fileRegistration;
  _fileOperationOptions.didRename = fileRegistration;
  _fileOperationOptions.didDelete = fileRegistration;
}

const ::lsp::FileOperationOptions &
DefaultFileOperationHandler::fileOperationOptions() const noexcept {
  return _fileOperationOptions;
}

void DefaultFileOperationHandler::didCreateFiles(
    const ::lsp::CreateFilesParams &params) {
  ::lsp::Array<::lsp::FileEvent> changes;
  for (const auto &file : params.files) {
    changes.push_back(::lsp::FileEvent{
        .uri = ::lsp::FileUri(::lsp::Uri::parse(file.uri)),
        .type = ::lsp::FileChangeType::Created,
    });
  }
  forward_file_changes(shared, std::move(changes));
}

void DefaultFileOperationHandler::didRenameFiles(
    const ::lsp::RenameFilesParams &params) {
  ::lsp::Array<::lsp::FileEvent> changes;
  for (const auto &file : params.files) {
    changes.push_back(::lsp::FileEvent{
        .uri = ::lsp::FileUri(::lsp::Uri::parse(file.oldUri)),
        .type = ::lsp::FileChangeType::Deleted,
    });
    changes.push_back(::lsp::FileEvent{
        .uri = ::lsp::FileUri(::lsp::Uri::parse(file.newUri)),
        .type = ::lsp::FileChangeType::Created,
    });
  }
  forward_file_changes(shared, std::move(changes));
}

void DefaultFileOperationHandler::didDeleteFiles(
    const ::lsp::DeleteFilesParams &params) {
  ::lsp::Array<::lsp::FileEvent> changes;
  for (const auto &file : params.files) {
    changes.push_back(::lsp::FileEvent{
        .uri = ::lsp::FileUri(::lsp::Uri::parse(file.uri)),
        .type = ::lsp::FileChangeType::Deleted,
    });
  }
  forward_file_changes(shared, std::move(changes));
}

} // namespace pegium
