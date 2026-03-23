#pragma once

#include <future>
#include <string>
#include <string_view>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace pegium {

/// Publishes one runtime failure event to the provided sink.
void publish_lsp_runtime_failure(observability::ObservabilitySink &sink,
                                 std::string category, std::string message);

/// Observes one background task and reports uncaught failures.
void observe_background_task(const services::SharedCoreServices &sharedServices,
                             std::string_view category,
                             std::future<void> future);

} // namespace pegium
