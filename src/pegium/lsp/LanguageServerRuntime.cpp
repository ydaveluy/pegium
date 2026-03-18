#include <pegium/lsp/LanguageServerRuntime.hpp>

#include <csignal>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <lsp/io/stream.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/Diagnostics.hpp>
#include <pegium/lsp/DocumentUpdateHandler.hpp>
#include <pegium/lsp/FileOperationHandler.hpp>
#include <pegium/lsp/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/LanguageServerRuntimeState.hpp>
#include <pegium/lsp/LanguageServerRequestHandlers.hpp>
#include <pegium/lsp/TextDocumentHandlers.hpp>
#include <pegium/lsp/WorkspaceAdapters.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

namespace {

void ignore_sigpipe_process_wide() {
#ifndef _WIN32
  static const bool ignored = []() {
    std::signal(SIGPIPE, SIG_IGN);
    return true;
  }();
  (void)ignored;
#endif
}

void ensure_initialized(const LanguageServerRuntimeState &runtimeState) {
  if (!runtimeState.initialized()) {
    throw ::lsp::RequestError(
        static_cast<int>(::lsp::ErrorCodes::ServerNotInitialized),
        "Server not initialized");
  }
}

std::optional<std::int64_t>
published_document_version(const std::shared_ptr<workspace::Document> &document) {
  if (document == nullptr) {
    return std::nullopt;
  }
  if (document->clientVersion().has_value()) {
    return document->clientVersion();
  }
  return static_cast<std::int64_t>(document->revision());
}

::lsp::RegistrationParams make_watched_files_registration() {
  ::lsp::FileSystemWatcher watcher{};
  watcher.globPattern = ::lsp::GlobPattern{"**/*"};

  ::lsp::DidChangeWatchedFilesRegistrationOptions options{};
  options.watchers.push_back(std::move(watcher));

  ::lsp::Registration registration{};
  registration.id = "pegium.workspace.didChangeWatchedFiles";
  registration.method =
      std::string(::lsp::notifications::Workspace_DidChangeWatchedFiles::Method);
  registration.registerOptions = ::lsp::toJson(std::move(options));

  ::lsp::RegistrationParams params{};
  params.registrations.push_back(std::move(registration));
  return params;
}

::lsp::TextDocumentSaveReason
to_lsp_save_reason(workspace::TextDocumentSaveReason reason) {
  using enum workspace::TextDocumentSaveReason;
  switch (reason) {
  case AfterDelay:
    return ::lsp::TextDocumentSaveReason::AfterDelay;
  case FocusOut:
    return ::lsp::TextDocumentSaveReason::FocusOut;
  case Manual:
  default:
    return ::lsp::TextDocumentSaveReason::Manual;
  }
}

workspace::TextDocumentEdit
to_workspace_text_edit(const ::lsp::TextEdit &edit) {
  return workspace::TextDocumentEdit{
      .range = workspace::TextDocumentContentChangeRange(edit.range.start,
                                                         edit.range.end),
      .newText = edit.newText,
  };
}

::lsp::FileOperationOptions
effective_file_operation_options(const FileOperationHandler &handler) {
  const auto &options = handler.fileOperationOptions();
  ::lsp::FileOperationOptions filtered{};
  if (handler.supportsDidCreateFiles() && options.didCreate.has_value()) {
    filtered.didCreate = options.didCreate;
  }
  if (handler.supportsWillCreateFiles() && options.willCreate.has_value()) {
    filtered.willCreate = options.willCreate;
  }
  if (handler.supportsDidRenameFiles() && options.didRename.has_value()) {
    filtered.didRename = options.didRename;
  }
  if (handler.supportsWillRenameFiles() && options.willRename.has_value()) {
    filtered.willRename = options.willRename;
  }
  if (handler.supportsDidDeleteFiles() && options.didDelete.has_value()) {
    filtered.didDelete = options.didDelete;
  }
  if (handler.supportsWillDeleteFiles() && options.willDelete.has_value()) {
    filtered.willDelete = options.willDelete;
  }
  return filtered;
}

} // namespace

void addConfigurationChangeHandler(::lsp::MessageHandler &messageHandler,
                                   const services::SharedServices &sharedServices,
                                   std::function<void()> ensureInitialized) {
  messageHandler.add<::lsp::notifications::Workspace_DidChangeConfiguration>(
      [&sharedServices,
       ensureInitialized = std::move(ensureInitialized)](
          const ::lsp::DidChangeConfigurationParams &params) {
        if (ensureInitialized) {
          ensureInitialized();
        }
        if (sharedServices.workspace.configurationProvider != nullptr) {
          sharedServices.workspace.configurationProvider->updateConfiguration(
              to_configuration_change_params(params));
        }
      });
}

void addDiagnosticsHandler(::lsp::MessageHandler &messageHandler,
                           services::SharedServices &sharedServices,
                           utils::DisposableStore &disposables) {
  if (sharedServices.workspace.documentBuilder == nullptr) {
    return;
  }

  disposables.add(sharedServices.workspace.documentBuilder->onUpdate(
      [&messageHandler,
       documents = sharedServices.workspace.documents.get()](
          std::span<const workspace::DocumentId>,
          std::span<const workspace::DocumentId> deletedDocumentIds) {
        if (documents == nullptr) {
          return;
        }
        for (const auto documentId : deletedDocumentIds) {
          const auto uri = documents->getDocumentUri(documentId);
          if (uri.empty()) {
            continue;
          }
          publish_diagnostics(&messageHandler,
                              workspace::DocumentDiagnosticsSnapshot{
                                  .uri = uri,
                                  .version = std::nullopt,
                                  .text = {},
                                  .diagnostics = {},
                              });
        }
      }));

  disposables.add(sharedServices.workspace.documentBuilder->onDocumentPhase(
      workspace::DocumentState::Validated,
      [&messageHandler](const std::shared_ptr<workspace::Document> &document) {
        if (document == nullptr) {
          return;
        }
        publish_diagnostics(&messageHandler,
                            workspace::DocumentDiagnosticsSnapshot{
                                .uri = document->uri,
                                .version = published_document_version(document),
                                .text = document->text(),
                                .diagnostics = document->diagnostics,
                            });
      }));
}

void addDocumentUpdateHandler(::lsp::MessageHandler &messageHandler,
                              services::SharedServices &sharedServices,
                              std::function<void()> ensureInitialized,
                              utils::DisposableStore &disposables) {
  if (sharedServices.workspace.textDocuments == nullptr ||
      sharedServices.lsp.documentUpdateHandler == nullptr) {
    return;
  }

  auto &documents = *sharedServices.workspace.textDocuments;
  auto &handler = *sharedServices.lsp.documentUpdateHandler;

  if (handler.supportsDidOpenDocument()) {
    disposables.add(documents.onDidOpen(
        [&handler](const workspace::TextDocumentEvent &event) {
          handler.didOpenDocument(event);
        }));
  }
  if (handler.supportsDidChangeContent()) {
    disposables.add(documents.onDidChangeContent(
        [&handler](const workspace::TextDocumentEvent &event) {
          handler.didChangeContent(event);
        }));
  }
  if (handler.supportsDidSaveDocument()) {
    disposables.add(documents.onDidSave(
        [&handler](const workspace::TextDocumentEvent &event) {
          handler.didSaveDocument(event);
        }));
  }
  if (handler.supportsDidCloseDocument()) {
    disposables.add(documents.onDidClose(
        [&handler](const workspace::TextDocumentEvent &event) {
          handler.didCloseDocument(event);
        }));
  }
  if (handler.supportsWillSaveDocument()) {
    disposables.add(documents.onWillSave(
        [&handler](const workspace::TextDocumentWillSaveEvent &event) {
          handler.willSaveDocument(TextDocumentWillSaveEvent{
              .document = event.document,
              .reason = to_lsp_save_reason(event.reason),
          });
        }));
  }
  if (handler.supportsWillSaveDocumentWaitUntil()) {
    disposables.add(documents.onWillSaveWaitUntil(
        [&handler](const workspace::TextDocumentWillSaveEvent &event) {
          std::vector<workspace::TextDocumentEdit> edits;
          for (const auto &edit :
               handler.willSaveDocumentWaitUntil(TextDocumentWillSaveEvent{
                   .document = event.document,
                   .reason = to_lsp_save_reason(event.reason),
               })) {
            edits.push_back(to_workspace_text_edit(edit));
          }
          return edits;
        }));
  }

  if (sharedServices.lsp.languageServer != nullptr) {
    auto canRegisterWatchedFiles = std::make_shared<bool>(false);
    disposables.add(sharedServices.lsp.languageServer->onInitialize(
        [canRegisterWatchedFiles](
            const ::lsp::InitializeParams &params) {
          *canRegisterWatchedFiles =
              params.capabilities.workspace.has_value() &&
              params.capabilities.workspace->didChangeWatchedFiles.has_value() &&
              params.capabilities.workspace->didChangeWatchedFiles
                  ->dynamicRegistration.value_or(false);
        }));
    disposables.add(sharedServices.lsp.languageServer->onInitialized(
        [&messageHandler, canRegisterWatchedFiles](
            const ::lsp::InitializedParams &) {
          if (!*canRegisterWatchedFiles) {
            return;
          }
          auto registration = make_watched_files_registration();
          (void)messageHandler
              .sendRequest<::lsp::requests::Client_RegisterCapability>(
                  std::move(registration),
                  [](::lsp::Client_RegisterCapabilityResult &&) {},
                  [](const ::lsp::ResponseError &) {});
        }));
  }

  addTextDocumentHandlers(messageHandler, documents, ensureInitialized);

  if (handler.supportsDidChangeWatchedFiles()) {
    messageHandler.add<::lsp::notifications::Workspace_DidChangeWatchedFiles>(
        [&handler, ensureInitialized = std::move(ensureInitialized)](
            const ::lsp::DidChangeWatchedFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          handler.didChangeWatchedFiles(params);
        });
  }
}

void addFileOperationHandler(::lsp::MessageHandler &messageHandler,
                             FileOperationHandler &fileOperationHandler,
                             const std::function<void()> &ensureInitialized) {
  const auto options = effective_file_operation_options(fileOperationHandler);

  if (options.didCreate.has_value()) {
    messageHandler.add<::lsp::notifications::Workspace_DidCreateFiles>(
        [&fileOperationHandler,
         ensureInitialized = ensureInitialized](const ::lsp::CreateFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          fileOperationHandler.didCreateFiles(params);
        });
  }

  if (options.didRename.has_value()) {
    messageHandler.add<::lsp::notifications::Workspace_DidRenameFiles>(
        [&fileOperationHandler,
         ensureInitialized = ensureInitialized](const ::lsp::RenameFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          fileOperationHandler.didRenameFiles(params);
        });
  }

  if (options.didDelete.has_value()) {
    messageHandler.add<::lsp::notifications::Workspace_DidDeleteFiles>(
        [&fileOperationHandler,
         ensureInitialized = ensureInitialized](const ::lsp::DeleteFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          fileOperationHandler.didDeleteFiles(params);
        });
  }

  if (options.willCreate.has_value()) {
    messageHandler.add<::lsp::requests::Workspace_WillCreateFiles>(
        [&fileOperationHandler,
         ensureInitialized = ensureInitialized](const ::lsp::CreateFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          const auto edit = fileOperationHandler.willCreateFiles(params);
          if (!edit.has_value()) {
            return ::lsp::Workspace_WillCreateFilesResult{};
          }
          return ::lsp::Workspace_WillCreateFilesResult{*edit};
        });
  }

  if (options.willRename.has_value()) {
    messageHandler.add<::lsp::requests::Workspace_WillRenameFiles>(
        [&fileOperationHandler,
         ensureInitialized = ensureInitialized](const ::lsp::RenameFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          const auto edit = fileOperationHandler.willRenameFiles(params);
          if (!edit.has_value()) {
            return ::lsp::Workspace_WillRenameFilesResult{};
          }
          return ::lsp::Workspace_WillRenameFilesResult{*edit};
        });
  }

  if (options.willDelete.has_value()) {
    messageHandler.add<::lsp::requests::Workspace_WillDeleteFiles>(
        [&fileOperationHandler,
         ensureInitialized = ensureInitialized](
            const ::lsp::DeleteFilesParams &params) {
          if (ensureInitialized) {
            ensureInitialized();
          }
          const auto edit = fileOperationHandler.willDeleteFiles(params);
          if (!edit.has_value()) {
            return ::lsp::Workspace_WillDeleteFilesResult{};
          }
          return ::lsp::Workspace_WillDeleteFilesResult{*edit};
        });
  }
}

int startLanguageServer(services::SharedServices &sharedServices,
                        const ServiceRequirements &serviceRequirements) {
  if (sharedServices.lsp.languageServer == nullptr ||
      sharedServices.lsp.connection == nullptr) {
    return 1;
  }

  ignore_sigpipe_process_wide();

  ::lsp::MessageHandler messageHandler(*sharedServices.lsp.connection);
  LanguageServerRuntimeState runtimeState;
  runtimeState.reset();
  LanguageServerHandlerContext handlerContext(*sharedServices.lsp.languageServer,
                                             sharedServices, runtimeState);

  utils::DisposableStore runtimeDisposables;

  addConfigurationChangeHandler(messageHandler, sharedServices,
                                [&runtimeState]() { ensure_initialized(runtimeState); });
  addDiagnosticsHandler(messageHandler, sharedServices, runtimeDisposables);
  addDocumentUpdateHandler(messageHandler, sharedServices,
                           [&runtimeState]() { ensure_initialized(runtimeState); },
                           runtimeDisposables);
  if (sharedServices.lsp.fileOperationHandler != nullptr) {
    addFileOperationHandler(messageHandler,
                            *sharedServices.lsp.fileOperationHandler,
                            [&runtimeState]() { ensure_initialized(runtimeState); });
  }
  addLanguageServerRequestHandlers(handlerContext, serviceRequirements,
                                   messageHandler);

  while (!runtimeState.exitRequested()) {
    try {
      messageHandler.processIncomingMessages();
    } catch (const ::lsp::ConnectionError &) {
      break;
    } catch (const ::lsp::io::Error &) {
      break;
    } catch (const std::exception &) {
      break;
    } catch (...) {
      break;
    }
  }

  const bool shutdownRequested = runtimeState.shutdownRequested();
  if (shutdownRequested) {
    runtimeState.waitForPendingRequests();
  }

  runtimeDisposables.dispose();
  runtimeState.reset();
  return shutdownRequested ? 0 : 1;
}

} // namespace pegium::lsp
