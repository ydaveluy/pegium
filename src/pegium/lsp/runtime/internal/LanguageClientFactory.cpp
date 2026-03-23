#include <pegium/lsp/runtime/internal/LanguageClientFactory.hpp>

#include <future>
#include <memory>
#include <string>
#include <utility>

#include <lsp/error.h>
#include <lsp/messages.h>

#include <pegium/lsp/support/JsonValue.hpp>
#include <pegium/lsp/runtime/internal/RuntimeObservability.hpp>

namespace pegium {
namespace {

class MessageHandlerLanguageClient final : public LanguageClient {
public:
  MessageHandlerLanguageClient(::lsp::MessageHandler &messageHandler,
                               observability::ObservabilitySink &sink) noexcept
      : _messageHandler(messageHandler), _sink(sink) {}

  [[nodiscard]] std::future<void>
  registerCapability(::lsp::RegistrationParams params) override {
    if (params.registrations.empty()) {
      std::promise<void> promise;
      promise.set_value();
      return promise.get_future();
    }

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    try {
      (void)_messageHandler.sendRequest<::lsp::requests::Client_RegisterCapability>(
          std::move(params),
          [promise](::lsp::Client_RegisterCapabilityResult &&) mutable {
            promise->set_value();
          },
          [this, promise](const ::lsp::ResponseError &error) mutable {
            _sink.publish(observability::Observation{
                .severity = observability::ObservationSeverity::Error,
                .code = observability::ObservationCode::LspRuntimeBackgroundTaskFailed,
                .message =
                    "Language client capability registration failed: " +
                    std::string(error.message()),
                .category = "client/registerCapability",
            });
            promise->set_value();
          });
    } catch (const std::exception &error) {
      publish_lsp_runtime_failure(
          _sink, "client/registerCapability",
          "Language client capability registration failed: " +
              std::string(error.what()));
      promise->set_value();
    } catch (...) {
      publish_lsp_runtime_failure(_sink, "client/registerCapability",
                                  "Language client capability registration failed.");
      promise->set_value();
    }
    return future;
  }

  [[nodiscard]] std::future<std::vector<services::JsonValue>>
  fetchConfiguration(::lsp::ConfigurationParams params) override {
    if (params.items.empty()) {
      std::promise<std::vector<services::JsonValue>> promise;
      promise.set_value({});
      return promise.get_future();
    }

    auto promise =
        std::make_shared<std::promise<std::vector<services::JsonValue>>>();
    auto future = promise->get_future();
    try {
      (void)_messageHandler.sendRequest<::lsp::requests::Workspace_Configuration>(
          std::move(params),
          [promise](::lsp::Workspace_ConfigurationResult &&result) mutable {
            std::vector<services::JsonValue> values;
            values.reserve(result.size());
            for (const auto &value : result) {
              values.push_back(from_lsp_any(value));
            }
            promise->set_value(std::move(values));
          },
          [this, promise](const ::lsp::ResponseError &error) mutable {
            _sink.publish(observability::Observation{
                .severity = observability::ObservationSeverity::Error,
                .code = observability::ObservationCode::LspRuntimeBackgroundTaskFailed,
                .message = "Language client configuration fetch failed: " +
                           std::string(error.message()),
                .category = "workspace/configuration",
            });
            promise->set_value({});
          });
    } catch (const std::exception &error) {
      publish_lsp_runtime_failure(
          _sink, "workspace/configuration",
          "Language client configuration fetch failed: " +
              std::string(error.what()));
      promise->set_value({});
    } catch (...) {
      publish_lsp_runtime_failure(_sink, "workspace/configuration",
                                  "Language client configuration fetch failed.");
      promise->set_value({});
    }
    return future;
  }

private:
  ::lsp::MessageHandler &_messageHandler;
  observability::ObservabilitySink &_sink;
};

} // namespace

std::unique_ptr<LanguageClient> make_message_handler_language_client(
    ::lsp::MessageHandler &messageHandler,
    observability::ObservabilitySink &observabilitySink) {
  return std::make_unique<MessageHandlerLanguageClient>(messageHandler,
                                                        observabilitySink);
}

} // namespace pegium
