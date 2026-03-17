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

#include <pegium/lsp/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/ServiceRequirements.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::lsp {

std::shared_ptr<workspace::Document>
get_document(const services::SharedServices &sharedServices,
             std::string_view uri);

::lsp::Range offset_to_range(const workspace::Document &document,
                             TextOffset begin, TextOffset end);

void ensure_initialized(LanguageServerHandlerContext &server);

std::optional<::lsp::CodeActionKindEnum>
to_lsp_code_action_kind(std::string_view kind);

std::shared_ptr<workspace::Document>
ensure_document_loaded(services::SharedServices &sharedServices,
                       std::string_view uri,
                       ServiceRequirement requiredState =
                           workspace::DocumentState::Parsed,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

ServiceRequirement requirement_or(
    const std::optional<ServiceRequirement> &requirement,
    ServiceRequirement defaultRequirement);

void wait_until_phase(services::SharedServices &sharedServices,
                      const utils::CancellationToken &cancelToken,
                      std::optional<std::string_view> uri,
                      ServiceRequirement requiredState);

std::string request_key_from_message_id(const ::lsp::MessageId &id);

std::string
request_key_from_cancel_id(const ::lsp::OneOf<int, ::lsp::String> &id);

std::string next_anonymous_request_key();

enum class GotoLinkKind : std::uint8_t {
  Declaration,
  Definition,
  TypeDefinition,
  Implementation,
};

[[nodiscard]] inline bool
goto_link_support_enabled(const workspace::InitializeCapabilities &capabilities,
                          GotoLinkKind kind) noexcept {
  switch (kind) {
  case GotoLinkKind::Declaration:
    return capabilities.declarationLinkSupport;
  case GotoLinkKind::Definition:
    return capabilities.definitionLinkSupport;
  case GotoLinkKind::TypeDefinition:
    return capabilities.typeDefinitionLinkSupport;
  case GotoLinkKind::Implementation:
    return capabilities.implementationLinkSupport;
  }
  return false;
}

template <typename Result, typename Payload>
[[nodiscard]] Result assign_request_result(Payload &&payload) {
  Result result{};
  result = std::forward<Payload>(payload);
  return result;
}

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

template <typename Result> struct wrap_vector_payload {
  template <typename Payload>
  [[nodiscard]] Result operator()(LanguageServerHandlerContext &,
                                  std::vector<Payload> value,
                                  const utils::CancellationToken &) const {
    return assign_request_result<Result>(std::move(value));
  }
};

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
struct wrap_optional_links {
  GotoLinkKind kind = GotoLinkKind::Definition;

  [[nodiscard]] Result
  operator()(LanguageServerHandlerContext &server,
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

template <typename T> struct async_value_type {
  using type = T;
};

template <typename T> struct async_value_type<std::future<T>> {
  using type = T;
};

template <typename T>
using async_value_type_t = typename async_value_type<std::decay_t<T>>::type;

template <typename T> [[nodiscard]] T resolve_async_result(T value) {
  return value;
}

template <typename T>
[[nodiscard]] T resolve_async_result(std::future<T> future) {
  if (!future.valid()) {
    return T{};
  }
  return future.get();
}

template <typename T, typename F>
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
auto dispatch_async(services::SharedServices &sharedServices, F &&task)
    -> std::future<std::invoke_result_t<F>> {
  (void)sharedServices;
  return std::async(std::launch::deferred, std::forward<F>(task));
}

template <typename Result, typename F>
auto make_async_request(LanguageServerHandlerContext &owner, F &&handler) {
  using Handler = std::decay_t<F>;
  auto handlerPtr = std::make_shared<Handler>(std::forward<F>(handler));
  return [&owner, handlerPtr](auto &&params) -> std::future<Result> {
    using Params = std::decay_t<decltype(params)>;
    Params ownedParams(std::forward<decltype(params)>(params));
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
          const auto cleanup = [&]() {
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
auto create_request_handler(LanguageServerHandlerContext &server,
                            services::SharedServices &sharedServices,
                            ServiceRequirement requiredState,
                            ServiceCall &&serviceCall, Adapter &&adapter) {
  using Call = std::decay_t<ServiceCall>;
  using ResultAdapter = std::decay_t<Adapter>;
  return make_async_request<Result>(
      server,
      [&server, &sharedServices, requiredState,
       serviceCall = Call(std::forward<ServiceCall>(serviceCall)),
       adapter = ResultAdapter(std::forward<Adapter>(adapter))](
          Params &&params, const utils::CancellationToken &cancelToken)
          -> std::future<Result> {
        ensure_initialized(server);
        const std::string uri = params.textDocument.uri.toString();
        wait_until_phase(sharedServices, cancelToken, uri, requiredState);
        return adapt_async_result<Result>(server,
                                          serviceCall(params, cancelToken),
                                          adapter, cancelToken);
      });
}

template <typename Result, typename Params, typename ServiceCall,
          typename Adapter>
auto create_item_request_handler(LanguageServerHandlerContext &server,
                                 services::SharedServices &sharedServices,
                                 ServiceRequirement requiredState,
                                 ServiceCall &&serviceCall, Adapter &&adapter) {
  using Call = std::decay_t<ServiceCall>;
  using ResultAdapter = std::decay_t<Adapter>;
  return make_async_request<Result>(
      server,
      [&server, &sharedServices, requiredState,
       serviceCall = Call(std::forward<ServiceCall>(serviceCall)),
       adapter = ResultAdapter(std::forward<Adapter>(adapter))](
          Params &&params, const utils::CancellationToken &cancelToken)
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
auto create_server_request_handler(LanguageServerHandlerContext &server,
                                   services::SharedServices &sharedServices,
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
          Params &&params, const utils::CancellationToken &cancelToken)
          -> std::future<Result> {
        ensure_initialized(server);
        wait_until_phase(sharedServices, cancelToken, std::nullopt,
                         requiredState);
        return adapt_async_result<Result>(server,
                                          serviceCall(params, cancelToken),
                                          adapter, cancelToken);
      });
}

} // namespace pegium::lsp
