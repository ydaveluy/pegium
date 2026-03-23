#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <pegium/core/observability/ObservabilitySink.hpp>

namespace pegium::observability {

/// Observability sink that writes formatted events to standard error.
class StderrObservabilitySink final : public ObservabilitySink {
public:
  void publish(const Observation &observation) noexcept override;

private:
  std::mutex _mutex;
};

/// Observability sink that forwards each event to multiple child sinks.
class FanoutObservabilitySink final : public ObservabilitySink {
public:
  FanoutObservabilitySink() = default;
  explicit FanoutObservabilitySink(
      std::vector<std::shared_ptr<ObservabilitySink>> sinks);

  void publish(const Observation &observation) noexcept override;

  void addSink(std::shared_ptr<ObservabilitySink> sink);
  [[nodiscard]] std::vector<std::shared_ptr<ObservabilitySink>>
  snapshot() const;

private:
  mutable std::mutex _mutex;
  std::vector<std::shared_ptr<ObservabilitySink>> _sinks;
};

} // namespace pegium::observability
