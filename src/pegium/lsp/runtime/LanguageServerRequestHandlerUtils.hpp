#pragma once

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/services/ServiceRequirements.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>
#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium {

/// Converts a document offset range to the corresponding LSP range.
::lsp::Range offset_to_range(const workspace::Document &document,
                             TextOffset begin, TextOffset end);

/// Throws when the language server has not completed initialization.
void ensure_initialized(const LanguageServerHandlerContext &server);

/// Maps a textual code-action kind to the matching LSP enum value.
std::optional<::lsp::CodeActionKindEnum>
to_lsp_code_action_kind(std::string_view kind);

/// Loads `uri` and waits until it reaches `requiredState`.
std::shared_ptr<workspace::Document>
ensure_document_loaded(pegium::SharedServices &sharedServices,
                       std::string_view uri,
                       ServiceRequirement requiredState =
                           workspace::DocumentState::Parsed,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

/// Waits until one document, or the whole workspace, reaches `requiredState`.
void wait_until_phase(pegium::SharedServices &sharedServices,
                      const utils::CancellationToken &cancelToken,
                      std::optional<std::string_view> uri,
                      ServiceRequirement requiredState);

/// Builds a stable cancellation key from an in-flight request id.
std::string request_key_from_message_id(const ::lsp::MessageId &id);

/// Builds a stable cancellation key from a cancel notification id.
std::string
request_key_from_cancel_id(const ::lsp::OneOf<int, ::lsp::String> &id);

/// Allocates a unique key for requests without a protocol id.
std::string next_anonymous_request_key();

/// Identifies goto-style requests that may return location links.
enum class GotoLinkKind : std::uint8_t {
  Declaration,
  Definition,
  TypeDefinition,
  Implementation,
};

/// Returns whether the client accepts `LocationLink` results for `kind`.
[[nodiscard]] inline bool
goto_link_support_enabled(const workspace::InitializeCapabilities &capabilities,
                          GotoLinkKind kind) noexcept {
  using enum GotoLinkKind;
  switch (kind) {
  case Declaration:
    return capabilities.declarationLinkSupport;
  case Definition:
    return capabilities.definitionLinkSupport;
  case TypeDefinition:
    return capabilities.typeDefinitionLinkSupport;
  case Implementation:
    return capabilities.implementationLinkSupport;
  }
  return false;
}

template <typename Result, typename Payload>
/// Wraps `payload` into the concrete LSP result carrier.
[[nodiscard]] Result assign_request_result(Payload &&payload) {
  Result result{};
  result = std::forward<Payload>(payload);
  return result;
}

/// Adapts an optional payload to nullable LSP responses.
template <typename Result> struct wrap_optional_payload {
  template <typename Payload>
  [[nodiscard]] Result
  operator()(LanguageServerHandlerContext &, std::optional<Payload> value,
             const utils::CancellationToken &) const {
    if (!value.has_value()) {
      return nullptr;
    }
    return assign_request_result<Result>(std::move(*value));
  }
};

/// Adapts vector payloads to the requested LSP response type.
template <typename Result> struct wrap_vector_payload {
  template <typename Payload>
  [[nodiscard]] Result operator()(LanguageServerHandlerContext &,
                                  std::vector<Payload> value,
                                  const utils::CancellationToken &) const {
    return assign_request_result<Result>(std::move(value));
  }
};

/// Converts empty vector payloads to `null` responses.
template <typename Result> struct wrap_empty_vector_as_null {
  template <typename Payload>
  [[nodiscard]] Result operator()(LanguageServerHandlerContext &,
                                  std::vector<Payload> value,
                                  const utils::CancellationToken &) const {
    if (value.empty()) {
      return nullptr;
    }
    return assign_request_result<Result>(std::move(value));
  }
};

template <typename Result, typename LocationPayload>
/// Downgrades link-based goto results when the client lacks link support.
struct wrap_optional_links {
  GotoLinkKind kind = GotoLinkKind::Definition;

  [[nodiscard]] Result
  operator()(const LanguageServerHandlerContext &server,
             std::optional<std::vector<::lsp::LocationLink>> links,
             const utils::CancellationToken &cancelToken) const {
    if (!links.has_value()) {
      return nullptr;
    }
    if (goto_link_support_enabled(server.initializeCapabilities(), kind)) {
      return assign_request_result<Result>(std::move(*links));
    }

    std::vector<::lsp::Location> locations;
    locations.reserve(links->size());
    for (auto &link : *links) {
      utils::throw_if_cancelled(cancelToken);
      ::lsp::Location location{};
      location.uri = std::move(link.targetUri);
      location.range = link.targetSelectionRange;
      locations.push_back(std::move(location));
    }

    LocationPayload payload{};
    payload = std::move(locations);
    return assign_request_result<Result>(std::move(payload));
  }
};

/// Falls back to the original payload when resolution returns no update.
template <typename T> struct wrap_resolved_or_original {
  T original;

  [[nodiscard]] T
  operator()(LanguageServerHandlerContext &, std::optional<T> value,
             const utils::CancellationToken &) & {
    if (value.has_value()) {
      return std::move(*value);
    }
    return original;
  }

  [[nodiscard]] T
  operator()(LanguageServerHandlerContext &, std::optional<T> value,
             const utils::CancellationToken &) && {
    if (value.has_value()) {
      return std::move(*value);
    }
    return std::move(original);
  }
};

/// Extracts the resolved value type from sync or future-based handlers.
template <typename T> struct async_value_type {
  using type = T;
};

template <typename T> struct async_value_type<std::future<T>> {
  using type = T;
};

template <typename T>
using async_value_type_t = typename async_value_type<std::decay_t<T>>::type;

/// Returns synchronous handler results unchanged.
template <typename T> [[nodiscard]] T resolve_async_result(T value) {
  return value;
}

template <typename T>
/// Resolves deferred handler results.
[[nodiscard]] T resolve_async_result(std::future<T> future) {
  if (!future.valid()) {
    return T{};
  }
  return future.get();
}

template <typename T, typename F>
/// Applies `mapper` after resolving a synchronous or deferred result.
auto map_async(T &&value, F &&mapper)
    -> std::future<
        std::invoke_result_t<F, async_value_type_t<std::decay_t<T>>>> {
  using In = async_value_type_t<std::decay_t<T>>;
  using Out = std::invoke_result_t<F, In>;
  return std::async(
      std::launch::deferred,
      [value = std::forward<T>(value), mapper = std::forward<F>(mapper)]()
          mutable -> Out {
        return mapper(resolve_async_result(std::move(value)));
      });
}

template <typename Result, typename T, typename Adapter>
/// Resolves `value` and adapts it to the wire-level `Result`.
auto adapt_async_result(LanguageServerHandlerContext &server, T &&value,
                        Adapter &&adapter,
                        const utils::CancellationToken &cancelToken)
    -> std::future<Result> {
  using ResultAdapter = std::decay_t<Adapter>;
  return map_async(
      std::forward<T>(value),
      [&server, adapter = ResultAdapter(std::forward<Adapter>(adapter)),
       cancelToken](auto resolvedValue) mutable -> Result {
        return std::move(adapter)(server, std::move(resolvedValue), cancelToken);
      });
}

template <typename F>
/// Dispatches background work for one LSP request.
auto dispatch_async(const pegium::SharedServices &sharedServices, F &&task)
    -> std::future<std::invoke_result_t<F>> {
  (void)sharedServices;
  return std::async(std::launch::deferred, std::forward<F>(task));
}

template <typename Result, typename F>
/// Wraps one handler with cancellation registration and cleanup.
auto make_async_request(LanguageServerHandlerContext &owner, F &&handler) {
  using Handler = std::decay_t<F>;
  auto handlerPtr = std::make_shared<Handler>(std::forward<F>(handler));
  return [&owner, handlerPtr]<typename Params>(
             Params &&params) -> std::future<Result> {
    using OwnedParams = std::decay_t<Params>;
    OwnedParams ownedParams(std::forward<Params>(params));
    auto cancellation = std::make_shared<utils::CancellationTokenSource>();
    std::string requestKey;
    try {
      requestKey =
          request_key_from_message_id(::lsp::MessageHandler::currentRequestId());
    } catch (...) {
      requestKey.clear();
    }
    if (requestKey.empty()) {
      requestKey = next_anonymous_request_key();
    }
    owner.registerRequestCancellation(requestKey, cancellation);
    return dispatch_async(
        owner.sharedServices(),
        [&owner, requestKey, cancellation, handlerPtr,
         ownedParams = std::move(ownedParams)]() mutable -> Result {
          const auto cleanup = [&owner, &requestKey, &cancellation]() {
            if (!requestKey.empty()) {
              owner.clearRequestCancellation(requestKey, cancellation);
            }
          };
          try {
            auto result =
                (*handlerPtr)(std::move(ownedParams), cancellation->get_token());
            cleanup();
            return resolve_async_result(std::move(result));
          } catch (const utils::OperationCancelled &) {
            cleanup();
            throw ::lsp::RequestError(::lsp::MessageError::RequestCancelled,
                                      "Operation cancelled");
          } catch (...) {
            cleanup();
            throw;
          }
        });
  };
}

template <typename Result, typename Params, typename ServiceCall,
          typename Adapter>
/// Creates a document-scoped request handler.
auto create_request_handler(LanguageServerHandlerContext &server,
                            pegium::SharedServices &sharedServices,
                            ServiceRequirement requiredState,
                            ServiceCall &&serviceCall, Adapter &&adapter) {
  using Call = std::decay_t<ServiceCall>;
  using ResultAdapter = std::decay_t<Adapter>;
  return make_async_request<Result>(
      server,
      [&server, &sharedServices, requiredState,
       serviceCall = Call(std::forward<ServiceCall>(serviceCall)),
       adapter = ResultAdapter(std::forward<Adapter>(adapter))](
          const Params &params, const utils::CancellationToken &cancelToken)
          -> std::future<Result> {
        ensure_initialized(server);
        const std::string uri = params.textDocument.uri.toString();
        wait_until_phase(sharedServices, cancelToken, uri, requiredState);
        if (get_services(*sharedServices.serviceRegistry, uri) == nullptr) {
          return std::async(std::launch::deferred,
                            []() -> Result { return Result{}; });
        }
        return adapt_async_result<Result>(server,
                                          serviceCall(params, cancelToken),
                                          adapter, cancelToken);
      });
}

template <typename Result, typename Params, typename ServiceCall,
          typename Adapter>
/// Creates an item-scoped request handler.
auto create_item_request_handler(LanguageServerHandlerContext &server,
                                 pegium::SharedServices &sharedServices,
                                 ServiceRequirement requiredState,
                                 ServiceCall &&serviceCall, Adapter &&adapter) {
  using Call = std::decay_t<ServiceCall>;
  using ResultAdapter = std::decay_t<Adapter>;
  return make_async_request<Result>(
      server,
      [&server, &sharedServices, requiredState,
       serviceCall = Call(std::forward<ServiceCall>(serviceCall)),
       adapter = ResultAdapter(std::forward<Adapter>(adapter))](
          const Params &params, const utils::CancellationToken &cancelToken)
          -> std::future<Result> {
        ensure_initialized(server);
        const std::string uri = params.item.uri.toString();
        wait_until_phase(sharedServices, cancelToken, uri, requiredState);
        return adapt_async_result<Result>(server,
                                          serviceCall(params, cancelToken),
                                          adapter, cancelToken);
      });
}

template <typename Result, typename Params, typename ServiceCall,
          typename Adapter>
/// Creates a workspace-scoped request handler.
auto create_server_request_handler(LanguageServerHandlerContext &server,
                                   pegium::SharedServices &sharedServices,
                                   ServiceRequirement requiredState,
                                   ServiceCall &&serviceCall,
                                   Adapter &&adapter) {
  using Call = std::decay_t<ServiceCall>;
  using ResultAdapter = std::decay_t<Adapter>;
  return make_async_request<Result>(
      server,
      [&server, &sharedServices, requiredState,
       serviceCall = Call(std::forward<ServiceCall>(serviceCall)),
       adapter = ResultAdapter(std::forward<Adapter>(adapter))](
          const Params &params, const utils::CancellationToken &cancelToken)
          -> std::future<Result> {
        ensure_initialized(server);
        wait_until_phase(sharedServices, cancelToken, std::nullopt,
                         requiredState);
        return adapt_async_result<Result>(server,
                                          serviceCall(params, cancelToken),
                                          adapter, cancelToken);
      });
}

} // namespace pegium
