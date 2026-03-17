#include <pegium/lsp/DefaultFileOperationHandler.hpp>

#include <utility>

#include <pegium/lsp/DocumentUpdateHandler.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

namespace {

::lsp::FileOperationOptions make_file_operation_options() {
  ::lsp::FileOperationFilter fileFilter{};
  fileFilter.pattern.glob = "**/*";
  fileFilter.scheme = "file";

  ::lsp::FileOperationRegistrationOptions fileRegistration{};
  fileRegistration.filters.push_back(std::move(fileFilter));

  ::lsp::FileOperationOptions options{};
  options.didCreate = fileRegistration;
  options.didRename = fileRegistration;
  options.didDelete = fileRegistration;
  return options;
}

void forward_file_changes(services::SharedServices &sharedServices,
                          ::lsp::Array<::lsp::FileEvent> changes) {
  if (sharedServices.lsp.documentUpdateHandler == nullptr || changes.empty()) {
    return;
  }

  ::lsp::DidChangeWatchedFilesParams params{};
  params.changes = std::move(changes);
  sharedServices.lsp.documentUpdateHandler->didChangeWatchedFiles(params);
}

} // namespace

DefaultFileOperationHandler::DefaultFileOperationHandler(
    services::SharedServices &sharedServices)
    : services::DefaultSharedLspService(sharedServices),
      _fileOperationOptions(make_file_operation_options()) {}

bool DefaultFileOperationHandler::supportsDidCreateFiles() const noexcept {
  return true;
}

bool DefaultFileOperationHandler::supportsDidRenameFiles() const noexcept {
  return true;
}

bool DefaultFileOperationHandler::supportsDidDeleteFiles() const noexcept {
  return true;
}

const ::lsp::FileOperationOptions &
DefaultFileOperationHandler::fileOperationOptions() const noexcept {
  return _fileOperationOptions;
}

void DefaultFileOperationHandler::didCreateFiles(
    const ::lsp::CreateFilesParams &params) {
  if (params.files.empty()) {
    return;
  }

  ::lsp::Array<::lsp::FileEvent> changes;
  for (const auto &file : params.files) {
    changes.push_back(::lsp::FileEvent{
        .uri = ::lsp::FileUri(::lsp::Uri::parse(file.uri)),
        .type = ::lsp::FileChangeType::Created,
    });
  }
  forward_file_changes(sharedServices, std::move(changes));
}

void DefaultFileOperationHandler::didRenameFiles(
    const ::lsp::RenameFilesParams &params) {
  if (params.files.empty()) {
    return;
  }

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
  forward_file_changes(sharedServices, std::move(changes));
}

void DefaultFileOperationHandler::didDeleteFiles(
    const ::lsp::DeleteFilesParams &params) {
  if (params.files.empty()) {
    return;
  }

  ::lsp::Array<::lsp::FileEvent> changes;
  for (const auto &file : params.files) {
    changes.push_back(::lsp::FileEvent{
        .uri = ::lsp::FileUri(::lsp::Uri::parse(file.uri)),
        .type = ::lsp::FileChangeType::Deleted,
    });
  }
  forward_file_changes(sharedServices, std::move(changes));
}

} // namespace pegium::lsp
