#include <pegium/lsp/runtime/LanguageServerRequestHandlerUtils.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <chrono>
#include <type_traits>
#include <utility>
#include <vector>

#include <pegium/lsp/services/ServiceAccess.hpp>
#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium {

namespace {

std::atomic<std::uint64_t> g_anonymousRequestCounter{0};

template <typename F>
decltype(auto)
with_workspace_read_lock(const pegium::SharedServices &sharedServices,
                         F &&action) {
  auto *lock = sharedServices.workspace.workspaceLock.get();
  assert(lock != nullptr);

  using Result = std::invoke_result_t<F>;
  if constexpr (std::is_void_v<Result>) {
    lock->read([task = std::forward<F>(action)]() mutable { task(); }).get();
    return;
  } else {
    std::optional<Result> result;
    lock->read([&result, task = std::forward<F>(action)]() mutable {
      result.emplace(task());
    }).get();
    Result value = std::move(result).value();
    return value;
  }
}

} // namespace

::lsp::Range offset_to_range(const workspace::Document &document,
                             TextOffset begin, TextOffset end) {
  const auto &textDocument = document.textDocument();
  ::lsp::Range range{};
  range.start = textDocument.positionAt(begin);
  range.end = textDocument.positionAt(end >= begin ? end : begin);
  return range;
}

void ensure_initialized(const LanguageServerHandlerContext &server) {
  if (!server.initialized()) {
    throw ::lsp::RequestError(
        static_cast<int>(::lsp::ErrorCodes::ServerNotInitialized),
        "Server not initialized");
  }
}

std::optional<::lsp::CodeActionKindEnum>
to_lsp_code_action_kind(std::string_view kind) {
  using enum ::lsp::CodeActionKind;
  if (kind.empty()) {
    return std::nullopt;
  }
  if (kind == "quickfix") {
    return QuickFix;
  }
  if (kind == "refactor") {
    return Refactor;
  }
  if (kind == "refactor.extract") {
    return RefactorExtract;
  }
  if (kind == "refactor.inline") {
    return RefactorInline;
  }
  if (kind == "refactor.rewrite") {
    return RefactorRewrite;
  }
  if (kind == "source") {
    return Source;
  }
  if (kind == "source.organizeImports") {
    return SourceOrganizeImports;
  }
  if (kind == "source.fixAll") {
    return SourceFixAll;
  }
  return std::nullopt;
}

std::shared_ptr<workspace::Document>
ensure_document_loaded(pegium::SharedServices &sharedServices,
                       std::string_view uri, ServiceRequirement requiredState,
                       const utils::CancellationToken &cancelToken) {
  if (auto document =
          with_workspace_read_lock(sharedServices, [&sharedServices, uri]() {
            return sharedServices.workspace.documents->getDocument(uri);
          });
      document != nullptr) {
    return document;
  }
  if (const auto *services =
          get_services(*sharedServices.serviceRegistry, uri);
      services == nullptr) {
    return nullptr;
  }

  auto created =
      sharedServices.workspace.documents->getOrCreateDocument(uri, cancelToken);
  workspace::BuildOptions options;
  const auto &languageId = created->textDocument().languageId();
  if (!languageId.empty()) {
    if (const auto validation =
            sharedServices.workspace.configurationProvider->getConfiguration(
                languageId, "validation");
        validation.has_value()) {
      (void)workspace::readValidationOption(*validation, options.validation);
    }
  }
  if (requiredState.state >= workspace::DocumentState::Validated &&
      (std::holds_alternative<std::monostate>(options.validation) ||
       (std::holds_alternative<bool>(options.validation) &&
        !std::get<bool>(options.validation)))) {
    options.validation = true;
  }

  const std::array<std::shared_ptr<workspace::Document>, 1> documents{created};
  sharedServices.workspace.documentBuilder->build(documents, options,
                                                  cancelToken);
  return created;
}

void wait_until_phase(pegium::SharedServices &sharedServices,
                      const utils::CancellationToken &cancelToken,
                      std::optional<std::string_view> uri,
                      ServiceRequirement requiredState) {
  constexpr auto kWaitStep = std::chrono::milliseconds(10);
  const auto ready = sharedServices.workspace.workspaceManager->ready();
  while (ready.wait_for(kWaitStep) != std::future_status::ready) {
    utils::throw_if_cancelled(cancelToken);
  }
  utils::throw_if_cancelled(cancelToken);
  ready.get();

  try {
    std::shared_ptr<workspace::Document> document;
    if (uri.has_value()) {
      document = ensure_document_loaded(sharedServices, *uri, requiredState,
                                        cancelToken);
    }

    if (uri.has_value() &&
        requiredState.type == ServiceRequirement::Type::Document) {
      if (document == nullptr) {
        throw utils::LanguageServerError(
            std::format("No document found for URI: {}", *uri));
      }
      (void)sharedServices.workspace.documentBuilder->waitUntil(
          requiredState.state, document->id, cancelToken);
      return;
    }

    sharedServices.workspace.documentBuilder->waitUntil(requiredState.state,
                                                        cancelToken);
  } catch (const ::lsp::RequestError &) {
    throw;
  } catch (const std::exception &error) {
    if (dynamic_cast<const utils::OperationCancelled *>(&error) != nullptr) {
      throw;
    }
    throw ::lsp::RequestError(::lsp::MessageError::RequestFailed, error.what());
  }
}

std::string request_key_from_message_id(const ::lsp::MessageId &id) {
  return std::visit(
      []<typename Value>(const Value &value) -> std::string {
        using DecayedValue = std::decay_t<Value>;
        if constexpr (std::is_same_v<DecayedValue, std::nullptr_t>) {
          return {};
        } else if constexpr (std::is_integral_v<DecayedValue>) {
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

} // namespace pegium
