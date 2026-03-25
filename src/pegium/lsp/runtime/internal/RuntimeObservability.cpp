#include <pegium/lsp/runtime/internal/RuntimeObservability.hpp>

#include <exception>
#include <thread>
#include <utility>

#include <pegium/core/observability/ObservabilitySink.hpp>

namespace pegium {

void publish_lsp_runtime_failure(observability::ObservabilitySink &sink,
                                 std::string category, std::string message) {
  sink.publish(observability::Observation{
      .severity = observability::ObservationSeverity::Error,
      .code = observability::ObservationCode::LspRuntimeBackgroundTaskFailed,
      .message = std::move(message),
      .category = std::move(category),
  });
}

void observe_background_task(const pegium::SharedCoreServices &sharedServices,
                             std::string_view category,
                             std::future<void> future) {
  if (!future.valid()) {
    return;
  }

  auto *shared = &sharedServices;
  std::string ownedCategory(category);
  std::thread([shared, category = std::move(ownedCategory),
               future = std::move(future)]() mutable {
    try {
      future.get();
    } catch (const std::exception &error) {
      publish_lsp_runtime_failure(
          *shared->observabilitySink, std::move(category),
          "LSP background task failed: " + std::string(error.what()));
    } catch (...) {
      publish_lsp_runtime_failure(*shared->observabilitySink,
                                  std::move(category),
                                  "LSP background task failed.");
    }
  }).detach();
}

} // namespace pegium
