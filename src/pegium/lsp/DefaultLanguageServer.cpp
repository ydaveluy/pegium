#include <pegium/lsp/DefaultLanguageServer.hpp>

#include <pegium/lsp/LanguageServerFeatures.hpp>
#include <pegium/lsp/ServiceAccess.hpp>
#include <pegium/lsp/WorkspaceAdapters.hpp>

#include <algorithm>
#include <format>
#include <lsp/messages.h>

#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>

#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Disposable.hpp>

namespace pegium::lsp {

namespace {

template <class Predicate>
bool any_language_supports(const services::SharedServices &sharedServices,
                           Predicate predicate) {
  if (sharedServices.serviceRegistry == nullptr) {
    return false;
  }
  for (const auto *coreServices : sharedServices.serviceRegistry->all()) {
    const auto *services = as_services(coreServices);
    if (services != nullptr && predicate(*services)) {
      return true;
    }
  }
  return false;
}

template <typename T> bool has_provider(T *provider) {
  return provider != nullptr;
}

template <typename Accessor>
bool any_language_has_provider(const services::SharedServices &sharedServices,
                               Accessor accessor) {
  return any_language_supports(sharedServices, [&](const auto &services) {
    return has_provider(accessor(services));
  });
}

bool has_file_operation_options(const ::lsp::FileOperationOptions &options) {
  return options.didCreate.has_value() || options.willCreate.has_value() ||
         options.didRename.has_value() || options.willRename.has_value() ||
         options.didDelete.has_value() || options.willDelete.has_value();
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

template <typename Callback>
void for_each_language_service(const services::SharedServices &sharedServices,
                               Callback callback) {
  if (sharedServices.serviceRegistry == nullptr) {
    return;
  }
  for (const auto *coreServices : sharedServices.serviceRegistry->all()) {
    const auto *services = as_services(coreServices);
    if (services != nullptr) {
      callback(*services);
    }
  }
}

template <typename OptArray>
void insert_strings(std::set<std::string> &target, const OptArray &values) {
  if (!values.has_value()) {
    return;
  }
  for (const auto &value : *values) {
    target.emplace(value);
  }
}

::lsp::Array<::lsp::String> to_lsp_array(const std::set<std::string> &values) {
  ::lsp::Array<::lsp::String> array;
  array.reserve(values.size());
  for (const auto &value : values) {
    array.push_back(value);
  }
  return array;
}

std::optional<::lsp::CompletionOptions>
merge_completion_options(const services::SharedServices &sharedServices) {
  bool hasProvider = false;
  std::set<std::string> triggerCharacters;
  std::set<std::string> allCommitCharacters;
  bool resolveProvider = false;
  std::optional<::lsp::CompletionOptionsCompletionItem> completionItem;

  for_each_language_service(sharedServices, [&](const auto &services) {
    const auto *provider = services.lsp.completionProvider.get();
    if (provider == nullptr) {
      return;
    }
    hasProvider = true;
    const auto options = provider->completionOptions();
    if (!options.has_value()) {
      return;
    }
    insert_strings(triggerCharacters, options->triggerCharacters);
    insert_strings(allCommitCharacters, options->allCommitCharacters);
    if (options->resolveProvider.has_value() && *options->resolveProvider) {
      resolveProvider = true;
    }
    if (options->completionItem.has_value() && !completionItem.has_value()) {
      completionItem = *options->completionItem;
    }
  });

  if (!hasProvider) {
    return std::nullopt;
  }

  ::lsp::CompletionOptions merged{};
  if (!triggerCharacters.empty()) {
    merged.triggerCharacters = to_lsp_array(triggerCharacters);
  }
  if (!allCommitCharacters.empty()) {
    merged.allCommitCharacters = to_lsp_array(allCommitCharacters);
  }
  if (resolveProvider) {
    merged.resolveProvider = true;
  }
  if (completionItem.has_value()) {
    merged.completionItem = *completionItem;
  }
  return merged;
}

std::optional<::lsp::SignatureHelpOptions>
merge_signature_help_options(const services::SharedServices &sharedServices) {
  std::set<std::string> triggerCharacters;
  std::set<std::string> retriggerCharacters;

  for_each_language_service(sharedServices, [&](const auto &services) {
    const auto *provider = services.lsp.signatureHelp.get();
    if (provider == nullptr) {
      return;
    }
    const auto options = provider->signatureHelpOptions();
    insert_strings(triggerCharacters, options.triggerCharacters);
    insert_strings(retriggerCharacters, options.retriggerCharacters);
  });

  if (triggerCharacters.empty()) {
    return std::nullopt;
  }

  ::lsp::SignatureHelpOptions merged{};
  merged.triggerCharacters = to_lsp_array(triggerCharacters);
  if (!retriggerCharacters.empty()) {
    merged.retriggerCharacters = to_lsp_array(retriggerCharacters);
  }
  return merged;
}

std::optional<::lsp::DocumentOnTypeFormattingOptions>
find_format_on_type_options(const services::SharedServices &sharedServices) {
  std::optional<::lsp::DocumentOnTypeFormattingOptions> result;
  for_each_language_service(sharedServices, [&](const auto &services) {
    if (result.has_value()) {
      return;
    }
    if (services.lsp.formatter == nullptr) {
      return;
    }
    result = services.lsp.formatter->formatOnTypeOptions();
  });
  return result;
}

void merge_indexed_strings(std::vector<std::string> &merged,
                           const ::lsp::Array<::lsp::String> &incoming,
                           std::string_view label) {
  if (merged.size() < incoming.size()) {
    merged.resize(incoming.size());
  }
  for (std::size_t index = 0; index < incoming.size(); ++index) {
    const auto &value = incoming[index];
    if (merged[index].empty()) {
      merged[index] = value;
    } else if (merged[index] != value) {
      throw std::runtime_error(std::format(
          "Cannot merge '{}' and '{}' {}. They use the same index {}.",
          merged[index], std::string(value), label, index));
    }
  }
}

std::optional<::lsp::SemanticTokensOptions>
merge_semantic_token_options(const services::SharedServices &sharedServices) {
  bool hasProvider = false;
  std::vector<std::string> tokenTypes;
  std::vector<std::string> tokenModifiers;
  bool full = true;
  bool delta = true;
  bool range = true;

  for_each_language_service(sharedServices, [&](const auto &services) {
    const auto *provider = services.lsp.semanticTokenProvider.get();
    if (provider == nullptr) {
      return;
    }
    hasProvider = true;
    const auto options = provider->semanticTokensOptions();
    merge_indexed_strings(tokenTypes, options.legend.tokenTypes, "token types");
    merge_indexed_strings(tokenModifiers, options.legend.tokenModifiers,
                          "token modifier");

    if (!options.full.has_value()) {
      full = false;
    } else if (const auto *asBool = std::get_if<bool>(&*options.full)) {
      full = full && *asBool;
    } else if (const auto *asOptions =
                   std::get_if<::lsp::SemanticTokensOptionsFull>(&*options.full)) {
      const auto supportsDelta =
          asOptions->delta.has_value() ? *asOptions->delta : false;
      delta = delta && supportsDelta;
    }

    if (!options.range.has_value()) {
      range = false;
    } else if (const auto *asBool = std::get_if<bool>(&*options.range)) {
      range = range && *asBool;
    }
  });

  if (!hasProvider) {
    return std::nullopt;
  }

  ::lsp::SemanticTokensOptions merged{};
  for (const auto &tokenType : tokenTypes) {
    if (!tokenType.empty()) {
      merged.legend.tokenTypes.push_back(tokenType);
    }
  }
  for (const auto &tokenModifier : tokenModifiers) {
    if (!tokenModifier.empty()) {
      merged.legend.tokenModifiers.push_back(tokenModifier);
    }
  }
  merged.range = range;
  if (full) {
    ::lsp::SemanticTokensOptionsFull fullOptions{};
    fullOptions.delta = delta;
    merged.full = fullOptions;
  } else {
    merged.full = false;
  }
  return merged;
}

} // namespace

::lsp::InitializeResult
DefaultLanguageServer::initialize(const ::lsp::InitializeParams &params) {
  if (sharedServices.workspace.configurationProvider != nullptr) {
    sharedServices.workspace.configurationProvider->initialize(
        to_workspace_initialize_params(params));
  }
  if (sharedServices.workspace.workspaceManager != nullptr) {
    sharedServices.workspace.workspaceManager->initialize(
        to_workspace_initialize_params(params));
  }

  _onInitialize.emit(params);

  ::lsp::InitializeResult result{};
  ::lsp::TextDocumentSyncOptions textSync{};
  textSync.openClose = true;
  textSync.change = ::lsp::TextDocumentSyncKind::Incremental;
  textSync.save = sharedServices.lsp.documentUpdateHandler != nullptr &&
                  sharedServices.lsp.documentUpdateHandler
                      ->supportsDidSaveDocument();
  if (sharedServices.lsp.documentUpdateHandler != nullptr) {
    textSync.willSave =
        sharedServices.lsp.documentUpdateHandler->supportsWillSaveDocument();
    textSync.willSaveWaitUntil = sharedServices.lsp.documentUpdateHandler
                                     ->supportsWillSaveDocumentWaitUntil();
  }

  const auto completionOptions = merge_completion_options(sharedServices);
  const auto signatureHelpOptions = merge_signature_help_options(sharedServices);
  const auto formattingOnTypeOptions = find_format_on_type_options(sharedServices);
  const auto semanticTokensOptions =
      merge_semantic_token_options(sharedServices);

  const bool hasCompletionProvider = completionOptions.has_value();
  const bool hasHoverProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.hoverProvider.get(); });
  const bool hasSignatureHelpProvider = signatureHelpOptions.has_value();
  const bool hasDeclarationProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.declarationProvider.get(); });
  const bool hasDefinitionProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.definitionProvider.get(); });
  const bool hasTypeDefinitionProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.typeProvider.get();
      });
  const bool hasImplementationProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.implementationProvider.get();
      });
  const bool hasReferencesProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.referencesProvider.get(); });
  const bool hasDocumentHighlightProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.documentHighlightProvider.get();
      });
  const bool hasDocumentSymbolProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.documentSymbolProvider.get();
      });
  const bool hasCodeActionProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.codeActionProvider.get(); });
  const bool hasCodeLensProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.codeLensProvider.get(); });
  const bool hasDocumentLinkProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.documentLinkProvider.get();
      });
  const bool hasFormattingProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.formatter.get();
      });
  const bool hasFoldingRangeProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.foldingRangeProvider.get();
      });
  const bool hasSelectionRangeProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.selectionRangeProvider.get();
      });
  const bool hasCallHierarchyProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.callHierarchyProvider.get();
      });
  const bool hasTypeHierarchyProvider = any_language_has_provider(
      sharedServices, [](const auto &services) {
        return services.lsp.typeHierarchyProvider.get();
      });
  const bool hasInlayHintProvider = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.inlayHintProvider.get(); });
  const bool hasSemanticTokenProvider = semanticTokensOptions.has_value();
  const bool hasWorkspaceSymbols =
      sharedServices.lsp.workspaceSymbolProvider != nullptr;
  const auto executeCommands = pegium::lsp::getExecuteCommands(sharedServices);

  result.capabilities.positionEncoding = ::lsp::PositionEncodingKind::UTF16;
  result.capabilities.textDocumentSync = textSync;
  if (hasCompletionProvider) {
    result.capabilities.completionProvider = *completionOptions;
  }
  if (hasHoverProvider) {
    result.capabilities.hoverProvider = true;
  }
  if (hasSignatureHelpProvider) {
    result.capabilities.signatureHelpProvider = *signatureHelpOptions;
  }
  if (hasDeclarationProvider) {
    result.capabilities.declarationProvider = true;
  }
  if (hasDefinitionProvider) {
    result.capabilities.definitionProvider = true;
  }
  if (hasTypeDefinitionProvider) {
    result.capabilities.typeDefinitionProvider = true;
  }
  if (hasImplementationProvider) {
    result.capabilities.implementationProvider = true;
  }
  if (hasReferencesProvider) {
    result.capabilities.referencesProvider = true;
  }
  if (hasDocumentHighlightProvider) {
    result.capabilities.documentHighlightProvider = true;
  }
  if (hasDocumentSymbolProvider) {
    result.capabilities.documentSymbolProvider = true;
  }
  if (hasCodeActionProvider) {
    result.capabilities.codeActionProvider = true;
  }
  if (hasCodeLensProvider) {
    ::lsp::CodeLensOptions options{};
    options.resolveProvider = false;
    result.capabilities.codeLensProvider = std::move(options);
  }
  if (hasFoldingRangeProvider) {
    result.capabilities.foldingRangeProvider = true;
  }
  if (hasDocumentLinkProvider) {
    ::lsp::DocumentLinkOptions options{};
    options.resolveProvider = false;
    result.capabilities.documentLinkProvider = std::move(options);
  }
  if (hasWorkspaceSymbols) {
    ::lsp::WorkspaceSymbolOptions options{};
    options.resolveProvider =
        sharedServices.lsp.workspaceSymbolProvider->supportsResolveSymbol();
    result.capabilities.workspaceSymbolProvider = std::move(options);
  }
  if (hasFormattingProvider) {
    result.capabilities.documentFormattingProvider = true;
    result.capabilities.documentRangeFormattingProvider = true;
  }
  if (formattingOnTypeOptions.has_value()) {
    result.capabilities.documentOnTypeFormattingProvider =
        *formattingOnTypeOptions;
  }
  if (hasSelectionRangeProvider) {
    result.capabilities.selectionRangeProvider = true;
  }
  const bool hasRename = any_language_has_provider(
      sharedServices,
      [](const auto &services) { return services.lsp.renameProvider.get(); });
  if (hasRename) {
    ::lsp::RenameOptions renameOptions{};
    renameOptions.prepareProvider = true;
    result.capabilities.renameProvider = std::move(renameOptions);
  }
  if (!executeCommands.empty()) {
    ::lsp::ExecuteCommandOptions options{};
    options.commands = executeCommands;
    result.capabilities.executeCommandProvider = std::move(options);
  }
  if (hasCallHierarchyProvider) {
    result.capabilities.callHierarchyProvider = ::lsp::CallHierarchyOptions{};
  }
  if (hasTypeHierarchyProvider) {
    result.capabilities.typeHierarchyProvider = ::lsp::TypeHierarchyOptions{};
  }
  if (hasInlayHintProvider) {
    ::lsp::InlayHintOptions options{};
    options.resolveProvider = false;
    result.capabilities.inlayHintProvider = std::move(options);
  }
  if (hasSemanticTokenProvider) {
    result.capabilities.semanticTokensProvider = *semanticTokensOptions;
  }

  ::lsp::ServerCapabilitiesWorkspace workspaceCapabilities{};
  ::lsp::WorkspaceFoldersServerCapabilities workspaceFolders{};
  workspaceFolders.supported = true;
  workspaceCapabilities.workspaceFolders = std::move(workspaceFolders);
  if (sharedServices.lsp.fileOperationHandler != nullptr) {
    const auto options = effective_file_operation_options(
        *sharedServices.lsp.fileOperationHandler);
    if (has_file_operation_options(options)) {
      workspaceCapabilities.fileOperations = options;
    }
  }
  result.capabilities.workspace = std::move(workspaceCapabilities);

  ::lsp::InitializeResultServerInfo info{};
  info.name = "pegium";
  result.serverInfo = std::move(info);

  return result;
}

void DefaultLanguageServer::initialized(const ::lsp::InitializedParams &params) {
  _onInitialized.emit(params);
}

utils::ScopedDisposable DefaultLanguageServer::onInitialize(
    std::function<void(const ::lsp::InitializeParams &)> callback) {
  return _onInitialize.on(std::move(callback));
}

utils::ScopedDisposable DefaultLanguageServer::onInitialized(
    std::function<void(const ::lsp::InitializedParams &)> callback) {
  return _onInitialized.on(std::move(callback));
}

} // namespace pegium::lsp
