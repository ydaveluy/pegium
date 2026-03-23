#include <pegium/core/observability/ObservabilitySinks.hpp>

#include <iostream>
#include <ranges>
#include <utility>

#include <pegium/core/observability/ObservationFormat.hpp>

namespace pegium::observability {

void StderrObservabilitySink::publish(const Observation &observation) noexcept {
  try {
    const auto formatted = detail::format_observation(observation);
    std::scoped_lock lock(_mutex);
    std::cerr << formatted << '\n';
  } catch (...) {
  }
}

FanoutObservabilitySink::FanoutObservabilitySink(
    std::vector<std::shared_ptr<ObservabilitySink>> sinks)
    : _sinks(std::move(sinks)) {}

void FanoutObservabilitySink::publish(const Observation &observation) noexcept {
  const auto sinks = snapshot();
  for (const auto &sink : sinks) {
    if (sink == nullptr) {
      continue;
    }
    try {
      sink->publish(observation);
    } catch (...) {
    }
  }
}

void FanoutObservabilitySink::addSink(std::shared_ptr<ObservabilitySink> sink) {
  if (sink == nullptr) {
    return;
  }
  std::scoped_lock lock(_mutex);
  if (std::ranges::find(_sinks, sink) != _sinks.end()) {
    return;
  }
  _sinks.push_back(std::move(sink));
}

std::vector<std::shared_ptr<ObservabilitySink>>
FanoutObservabilitySink::snapshot() const {
  std::scoped_lock lock(_mutex);
  return _sinks;
}

} // namespace pegium::observability
