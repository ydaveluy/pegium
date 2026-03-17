#include <pegium/lsp/LanguageServerRequestHandlerUtils.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pegium/lsp/ServiceAccess.hpp>
#include <pegium/utils/UriUtils.hpp>

namespace pegium::lsp {

namespace {

std::atomic<std::uint64_t> g_anonymousRequestCounter{0};

template <typename F>
decltype(auto) with_workspace_read_lock(
    const services::SharedServices &sharedServices, F &&action) {
  return workspace::run_with_workspace_read(
      sharedServices.workspace.workspaceLock.get(), std::forward<F>(action));
}

bool merge_validation_options_for_language(
    const services::SharedServices &sharedServices, std::string_view languageId,
    workspace::BuildOptions &options) {
  if (sharedServices.workspace.configurationProvider == nullptr ||
      languageId.empty()) {
    return false;
  }
  if (const auto validation =
          sharedServices.workspace.configurationProvider->getConfiguration(
              languageId, "validation");
      validation.has_value()) {
    (void)workspace::readValidationOptions(*validation, options.validation);
    return true;
  }
  return false;
}

} // namespace

std::shared_ptr<workspace::Document>
get_document(const services::SharedServices &sharedServices,
             std::string_view uri) {
  return with_workspace_read_lock(sharedServices, [&]() {
    if (sharedServices.workspace.documents == nullptr) {
      return std::shared_ptr<workspace::Document>{};
    }
    return sharedServices.workspace.documents->getDocument(uri);
  });
}

::lsp::Range offset_to_range(const workspace::Document &document,
                             TextOffset begin, TextOffset end) {
  ::lsp::Range range{};
  range.start = document.offsetToPosition(begin);
  range.end = document.offsetToPosition(end >= begin ? end : begin);
  return range;
}

void ensure_initialized(LanguageServerHandlerContext &server) {
  if (!server.initialized()) {
    throw ::lsp::RequestError(
        static_cast<int>(::lsp::ErrorCodes::ServerNotInitialized),
        "Server not initialized");
  }
}

std::optional<::lsp::CodeActionKindEnum>
to_lsp_code_action_kind(std::string_view kind) {
  if (kind.empty()) {
    return std::nullopt;
  }
  if (kind == "quickfix") {
    return ::lsp::CodeActionKind::QuickFix;
  }
  if (kind == "refactor") {
    return ::lsp::CodeActionKind::Refactor;
  }
  if (kind == "refactor.extract") {
    return ::lsp::CodeActionKind::RefactorExtract;
  }
  if (kind == "refactor.inline") {
    return ::lsp::CodeActionKind::RefactorInline;
  }
  if (kind == "refactor.rewrite") {
    return ::lsp::CodeActionKind::RefactorRewrite;
  }
  if (kind == "source") {
    return ::lsp::CodeActionKind::Source;
  }
  if (kind == "source.organizeImports") {
    return ::lsp::CodeActionKind::SourceOrganizeImports;
  }
  if (kind == "source.fixAll") {
    return ::lsp::CodeActionKind::SourceFixAll;
  }
  return std::nullopt;
}

std::shared_ptr<workspace::Document>
ensure_document_loaded(services::SharedServices &sharedServices,
                       std::string_view uri,
                       ServiceRequirement requiredState,
                       const utils::CancellationToken &cancelToken) {
  auto document = get_document(sharedServices, uri);
  if (document != nullptr) {
    return document;
  }
  if (sharedServices.serviceRegistry == nullptr) {
    return nullptr;
  }

  const auto *languageServices =
      get_services_for_uri(sharedServices.serviceRegistry.get(), uri);
  if (languageServices == nullptr || languageServices->languageId.empty()) {
    return nullptr;
  }

  const auto fileSystem = sharedServices.workspace.fileSystemProvider;
  if (!fileSystem) {
    return nullptr;
  }

  auto text = fileSystem->readFile(uri);
  if (!text.has_value()) {
    return nullptr;
  }

  if (sharedServices.workspace.documents == nullptr ||
      sharedServices.workspace.documentBuilder == nullptr) {
    return nullptr;
  }

  return workspace::run_with_workspace_write(
      sharedServices.workspace.workspaceLock.get(), cancelToken, [&]() {
        auto created =
            sharedServices.workspace.documents->getOrCreateDocument(uri, cancelToken);
        if (created == nullptr) {
          return std::shared_ptr<workspace::Document>{};
        }

        workspace::BuildOptions options;
        if (!created->languageId.empty()) {
          (void)merge_validation_options_for_language(sharedServices,
                                                      created->languageId,
                                                      options);
        }
        if (requiredState.state >= workspace::DocumentState::Validated) {
          options.validation.enabled = true;
        }

        const std::array<std::shared_ptr<workspace::Document>, 1> documents{
            created};
        (void)sharedServices.workspace.documentBuilder->build(
            documents, options, cancelToken);
        return created;
      });
}

ServiceRequirement requirement_or(
    const std::optional<ServiceRequirement> &requirement,
    ServiceRequirement defaultRequirement) {
  return requirement.value_or(defaultRequirement);
}

void wait_until_phase(services::SharedServices &sharedServices,
                      const utils::CancellationToken &cancelToken,
                      std::optional<std::string_view> uri,
                      ServiceRequirement requiredState) {
  if (sharedServices.workspace.workspaceManager != nullptr) {
    sharedServices.workspace.workspaceManager->waitUntilReady(cancelToken);
  }
  if (sharedServices.workspace.documentBuilder == nullptr) {
    return;
  }

  try {
    std::shared_ptr<workspace::Document> document;
    if (uri.has_value()) {
      document =
          ensure_document_loaded(sharedServices, *uri, requiredState, cancelToken);
    }

    if (uri.has_value() &&
        requiredState.type == ServiceRequirement::Type::Document) {
      if (document == nullptr) {
        throw std::runtime_error(
            std::format("No document found for URI: {}", *uri));
      }
      sharedServices.workspace.documentBuilder->waitUntil(requiredState.state,
                                                         document->id,
                                                         cancelToken);
      return;
    }

    sharedServices.workspace.documentBuilder->waitUntil(requiredState.state,
                                                       cancelToken);
  } catch (const utils::OperationCancelled &) {
    throw;
  } catch (const ::lsp::RequestError &) {
    throw;
  } catch (const std::exception &error) {
    throw ::lsp::RequestError(::lsp::MessageError::RequestFailed, error.what());
  }
}

std::string request_key_from_message_id(const ::lsp::MessageId &id) {
  return std::visit(
      [](const auto &value) -> std::string {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, std::nullptr_t>) {
          return {};
        } else if constexpr (std::is_integral_v<Value>) {
          return std::format("i:{}", value);
        } else {
          return std::format("s:{}", std::string(value));
        }
      },
      id);
}

std::string
request_key_from_cancel_id(const ::lsp::OneOf<int, ::lsp::String> &id) {
  if (std::holds_alternative<int>(id)) {
    return std::format("i:{}", std::get<int>(id));
  }
  return std::format("s:{}", std::string(std::get<::lsp::String>(id)));
}

std::string next_anonymous_request_key() {
  return std::format("anon:{}", ++g_anonymousRequestCounter);
}

} // namespace pegium::lsp
