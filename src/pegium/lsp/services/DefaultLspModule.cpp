#include <pegium/lsp/services/DefaultLspModule.hpp>

#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include <lsp/connection.h>
#include <lsp/json/json.h>
#include <lsp/jsonrpc/jsonrpc.h>

#include <pegium/core/observability/ObservationFormat.hpp>
#include <pegium/core/observability/ObservabilitySinks.hpp>
#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/lsp/code-actions/DefaultCodeActionProvider.hpp>
#include <pegium/lsp/completion/DefaultCompletionProvider.hpp>
#include <pegium/lsp/navigation/DefaultDefinitionProvider.hpp>
#include <pegium/lsp/navigation/DefaultDocumentHighlightProvider.hpp>
#include <pegium/lsp/symbols/DefaultDocumentSymbolProvider.hpp>
#include <pegium/lsp/workspace/DefaultDocumentUpdateHandler.hpp>
#include <pegium/lsp/ranges/DefaultFoldingRangeProvider.hpp>
#include <pegium/lsp/support/DefaultFuzzyMatcher.hpp>
#include <pegium/lsp/runtime/DefaultLanguageServer.hpp>
#include <pegium/lsp/navigation/DefaultReferencesProvider.hpp>
#include <pegium/lsp/navigation/DefaultRenameProvider.hpp>
#include <pegium/lsp/symbols/DefaultNodeKindProvider.hpp>
#include <pegium/lsp/workspace/DefaultTextDocuments.hpp>
#include <pegium/lsp/symbols/DefaultWorkspaceSymbolProvider.hpp>
#include <pegium/lsp/hover/MultilineCommentHoverProvider.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {
namespace {

constexpr std::string_view kWindowLogMessageMethod = "window/logMessage";
constexpr std::string_view kWindowShowMessageMethod = "window/showMessage";

int to_lsp_message_type(
    observability::ObservationSeverity severity) noexcept {
  switch (severity) {
  case observability::ObservationSeverity::Trace:
    return 4;
  case observability::ObservationSeverity::Info:
    return 3;
  case observability::ObservationSeverity::Warning:
    return 2;
  case observability::ObservationSeverity::Error:
    return 1;
  }
  return 3;
}

bool should_show_popup(observability::ObservationCode code) noexcept {
  return code == observability::ObservationCode::WorkspaceBootstrapFailed;
}

void send_window_message(::lsp::Connection &connection, std::string_view method,
                         int type, std::string message) {
  ::lsp::json::Object params;
  params["type"] = type;
  params["message"] = std::move(message);
  connection.writeMessage(
      ::lsp::Connection::Message(::lsp::jsonrpc::Message(
          ::lsp::jsonrpc::createNotification(
              method, ::lsp::json::Value(std::move(params))))));
}

class LspObservabilitySink final : public observability::ObservabilitySink {
public:
  explicit LspObservabilitySink(::lsp::Connection &connection) noexcept
      : _connection(&connection) {}

  void setConnection(::lsp::Connection &connection) noexcept {
    std::scoped_lock lock(_mutex);
    _connection = &connection;
  }

  void publish(const observability::Observation &observation) noexcept override {
    try {
      auto *connection = getConnection();
      if (connection == nullptr) {
        return;
      }
      const auto type = to_lsp_message_type(observation.severity);
      const auto message = observability::detail::format_observation(observation);
      send_window_message(*connection, kWindowLogMessageMethod, type, message);
      if (should_show_popup(observation.code)) {
        send_window_message(*connection, kWindowShowMessageMethod, type, message);
      }
    } catch (...) {
    }
  }

private:
  [[nodiscard]] ::lsp::Connection *getConnection() const noexcept {
    std::scoped_lock lock(_mutex);
    return _connection;
  }

  mutable std::mutex _mutex;
  ::lsp::Connection *_connection = nullptr;
};

std::shared_ptr<LspObservabilitySink>
find_lsp_observability_sink(
    const std::shared_ptr<observability::ObservabilitySink> &sink) {
  if (sink == nullptr) {
    return nullptr;
  }
  if (const auto lspSink = std::dynamic_pointer_cast<LspObservabilitySink>(sink);
      lspSink != nullptr) {
    return lspSink;
  }
  const auto fanout =
      std::dynamic_pointer_cast<observability::FanoutObservabilitySink>(sink);
  if (fanout == nullptr) {
    return nullptr;
  }
  for (const auto &child : fanout->snapshot()) {
    if (const auto lspSink =
            std::dynamic_pointer_cast<LspObservabilitySink>(child);
        lspSink != nullptr) {
      return lspSink;
    }
  }
  return nullptr;
}

void attach_lsp_observability_sink(SharedServices &sharedServices,
                                   ::lsp::Connection &connection) {
  if (const auto existing =
          find_lsp_observability_sink(sharedServices.observabilitySink);
      existing != nullptr) {
    existing->setConnection(connection);
    return;
  }

  auto lspSink = std::make_shared<LspObservabilitySink>(connection);
  if (const auto fanout =
          std::dynamic_pointer_cast<observability::FanoutObservabilitySink>(
              sharedServices.observabilitySink);
      fanout != nullptr) {
    fanout->addSink(lspSink);
    return;
  }

  sharedServices.observabilitySink =
      std::make_shared<observability::FanoutObservabilitySink>(
          std::vector<std::shared_ptr<observability::ObservabilitySink>>{
              sharedServices.observabilitySink, std::move(lspSink)});
}

} // namespace

void initializeSharedServicesForLanguageServer(
    SharedServices &sharedServices, ::lsp::Connection &connection) {
  services::installDefaultSharedCoreServices(sharedServices);
  sharedServices.lsp.languageClient.reset();
  attach_lsp_observability_sink(sharedServices, connection);
  installDefaultSharedLspServices(sharedServices);
}

void installDefaultLspServices(Services &services) {
  if (!services.lsp.completionProvider) {
    services.lsp.completionProvider =
        std::make_unique<DefaultCompletionProvider>(services);
  }
  if (!services.lsp.hoverProvider) {
    services.lsp.hoverProvider = std::make_unique<MultilineCommentHoverProvider>(services);
  }
  if (!services.lsp.documentSymbolProvider) {
    services.lsp.documentSymbolProvider =
        std::make_unique<DefaultDocumentSymbolProvider>(services);
  }
  if (!services.lsp.documentHighlightProvider) {
    services.lsp.documentHighlightProvider =
        std::make_unique<DefaultDocumentHighlightProvider>(services);
  }
  if (!services.lsp.foldingRangeProvider) {
    services.lsp.foldingRangeProvider =
        std::make_unique<DefaultFoldingRangeProvider>(services);
  }
  if (!services.lsp.definitionProvider) {
    services.lsp.definitionProvider =
        std::make_unique<DefaultDefinitionProvider>(services);
  }
  if (!services.lsp.referencesProvider) {
    services.lsp.referencesProvider =
        std::make_unique<DefaultReferencesProvider>(services);
  }
  if (!services.lsp.renameProvider) {
    services.lsp.renameProvider = std::make_unique<DefaultRenameProvider>(services);
  }
  if (!services.lsp.codeActionProvider) {
    services.lsp.codeActionProvider =
        std::make_unique<DefaultCodeActionProvider>();
  }
}

void installDefaultSharedLspServices(SharedServices &sharedServices) {
  if (!sharedServices.lsp.textDocuments) {
    sharedServices.lsp.textDocuments =
        std::make_shared<DefaultTextDocuments>();
  }
  sharedServices.workspace.textDocuments =
      sharedServices.lsp.textDocuments;
  if (!sharedServices.lsp.fuzzyMatcher) {
    sharedServices.lsp.fuzzyMatcher =
        std::make_unique<DefaultFuzzyMatcher>();
  }
  if (!sharedServices.lsp.languageServer) {
    sharedServices.lsp.languageServer =
        std::make_unique<DefaultLanguageServer>(sharedServices);
  }
  if (!sharedServices.lsp.documentUpdateHandler) {
    sharedServices.lsp.documentUpdateHandler =
        std::make_unique<DefaultDocumentUpdateHandler>(sharedServices);
  }
  if (!sharedServices.lsp.nodeKindProvider) {
    sharedServices.lsp.nodeKindProvider =
        std::make_unique<DefaultNodeKindProvider>(sharedServices);
  }
  if (!sharedServices.lsp.workspaceSymbolProvider) {
    sharedServices.lsp.workspaceSymbolProvider =
        std::make_unique<DefaultWorkspaceSymbolProvider>(sharedServices);
  }
}

} // namespace pegium
