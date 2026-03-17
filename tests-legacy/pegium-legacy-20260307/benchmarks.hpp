#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace pegium::test {

struct ParseBenchmarkResult {
  std::size_t bytes = 0;
  int iterations = 0;
  double elapsed_ms = 0.0;
  double mib_per_s = 0.0;
};

inline int getEnvInt(std::string_view name, int defaultValue,
                     int minValue = 0) {
  if (const auto *value = std::getenv(std::string{name}.c_str())) {
    const int parsed = std::atoi(value);
    return std::ranges::max(parsed, minValue);
  }
  return defaultValue;
}

inline std::optional<double> getEnvDouble(std::string_view name) {
  if (const auto *value = std::getenv(std::string{name}.c_str())) {
    char *end = nullptr;
    const auto parsed = std::strtod(value, &end);
    if (end != value && end && *end == '\0') {
      return parsed;
    }
  }
  return std::nullopt;
}

inline std::string makeBenchmarkKey(std::string_view key) {
  std::string normalized;
  normalized.reserve(key.size());
  for (const auto c : key) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      normalized.push_back(
          static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    } else {
      normalized.push_back('_');
    }
  }
  return normalized;
}

template <typename ParseFn>
ParseBenchmarkResult runParseBenchmark(std::string_view name,
                                       std::string_view input,
                                       ParseFn &&parseOnce,
                                       int iterationsOverride = -1,
                                       int warmupOverride = -1) {
  const int iterations =
      iterationsOverride > 0
          ? iterationsOverride
          : getEnvInt("PEGIUM_BENCH_ITERATIONS", 10, /*minValue*/ 1);
  const int warmup = warmupOverride >= 0
                         ? warmupOverride
                         : getEnvInt("PEGIUM_BENCH_WARMUP", 2, /*minValue*/ 0);
  const int samples =
      getEnvInt("PEGIUM_BENCH_SAMPLES", 1, /*minValue*/ 1);

  auto validate = [&](std::string_view stage) -> std::size_t {
    auto document = parseOnce(input);
    auto &result = document->parseResult;
    if (!result.value) {
      std::ostringstream os;
      os << "Benchmark '" << name << "' failed at " << stage
         << " (value=" << static_cast<bool>(result.value) << ')';
      throw std::runtime_error(os.str());
    }
    return input.size();
  };

  std::vector<double> throughputs;
  throughputs.reserve(static_cast<std::size_t>(samples));
  std::vector<double> elapsedSamples;
  elapsedSamples.reserve(static_cast<std::size_t>(samples));
  std::size_t parsedBytes = 0;

  for (int sample = 0; sample < samples; ++sample) {
    for (int i = 0; i < warmup; ++i) {
      (void)validate("warmup");
    }

    parsedBytes = 0;
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      parsedBytes += validate("iteration");
    }
    const auto end = std::chrono::high_resolution_clock::now();

    const double elapsedMs =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            end - start)
            .count();
    const double elapsedSec = elapsedMs / 1000.0;
    const double mib = static_cast<double>(parsedBytes) /
                       static_cast<double>(1024.0 * 1024.0);
    const double mibPerSec = elapsedSec > 0.0 ? (mib / elapsedSec) : 0.0;
    elapsedSamples.push_back(elapsedMs);
    throughputs.push_back(mibPerSec);
  }

  auto sortedThroughputs = throughputs;
  std::ranges::sort(sortedThroughputs);
  const std::size_t medianIndex = sortedThroughputs.size() / 2;
  const double mibPerSec =
      (sortedThroughputs.size() % 2 == 0)
          ? ((sortedThroughputs[medianIndex - 1] + sortedThroughputs[medianIndex]) /
             2.0)
          : sortedThroughputs[medianIndex];

  double elapsedMs = 0.0;
  for (const auto sampleElapsed : elapsedSamples) {
    elapsedMs += sampleElapsed;
  }
  elapsedMs /= static_cast<double>(elapsedSamples.size());

  std::cout << std::fixed << std::setprecision(2)
            << "[microbench] " << name << ": " << mibPerSec << " MiB/s ("
            << parsedBytes << " bytes, iterations=" << iterations
            << ", samples=" << samples << ", elapsed=" << elapsedMs
            << " ms avg)\n";

  return ParseBenchmarkResult{
      .bytes = parsedBytes,
      .iterations = iterations,
      .elapsed_ms = elapsedMs,
      .mib_per_s = mibPerSec,
  };
}

template <typename ExpectFn>
ParseBenchmarkResult runExpectBenchmark(std::string_view name,
                                        std::string_view input,
                                        std::size_t offset,
                                        ExpectFn &&expectOnce,
                                        int iterationsOverride = -1,
                                        int warmupOverride = -1) {
  const int iterations =
      iterationsOverride > 0
          ? iterationsOverride
          : getEnvInt("PEGIUM_BENCH_ITERATIONS", 10, /*minValue*/ 1);
  const int warmup = warmupOverride >= 0
                         ? warmupOverride
                         : getEnvInt("PEGIUM_BENCH_WARMUP", 2, /*minValue*/ 0);
  const int samples =
      getEnvInt("PEGIUM_BENCH_SAMPLES", 1, /*minValue*/ 1);

  auto validate = [&](std::string_view stage) -> std::size_t {
    auto result = expectOnce(input, offset);
    if (!result.reachedAnchor || result.frontier.empty()) {
      std::ostringstream os;
      os << "Benchmark '" << name << "' failed at " << stage
         << " (reachedAnchor=" << result.reachedAnchor
         << ", frontier=" << result.frontier.size() << ')';
      throw std::runtime_error(os.str());
    }
    return input.size();
  };

  std::vector<double> throughputs;
  throughputs.reserve(static_cast<std::size_t>(samples));
  std::vector<double> elapsedSamples;
  elapsedSamples.reserve(static_cast<std::size_t>(samples));
  std::size_t processedBytes = 0;

  for (int sample = 0; sample < samples; ++sample) {
    for (int i = 0; i < warmup; ++i) {
      (void)validate("warmup");
    }

    processedBytes = 0;
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      processedBytes += validate("iteration");
    }
    const auto end = std::chrono::high_resolution_clock::now();

    const double elapsedMs =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            end - start)
            .count();
    const double elapsedSec = elapsedMs / 1000.0;
    const double mib = static_cast<double>(processedBytes) /
                       static_cast<double>(1024.0 * 1024.0);
    const double mibPerSec = elapsedSec > 0.0 ? (mib / elapsedSec) : 0.0;
    elapsedSamples.push_back(elapsedMs);
    throughputs.push_back(mibPerSec);
  }

  auto sortedThroughputs = throughputs;
  std::ranges::sort(sortedThroughputs);
  const std::size_t medianIndex = sortedThroughputs.size() / 2;
  const double mibPerSec =
      (sortedThroughputs.size() % 2 == 0)
          ? ((sortedThroughputs[medianIndex - 1] + sortedThroughputs[medianIndex]) /
             2.0)
          : sortedThroughputs[medianIndex];

  double elapsedMs = 0.0;
  for (const auto sampleElapsed : elapsedSamples) {
    elapsedMs += sampleElapsed;
  }
  elapsedMs /= static_cast<double>(elapsedSamples.size());

  std::cout << std::fixed << std::setprecision(2)
            << "[microbench] " << name << ": " << mibPerSec << " MiB/s ("
            << processedBytes << " bytes, iterations=" << iterations
            << ", samples=" << samples << ", elapsed=" << elapsedMs
            << " ms avg)\n";

  return ParseBenchmarkResult{
      .bytes = processedBytes,
      .iterations = iterations,
      .elapsed_ms = elapsedMs,
      .mib_per_s = mibPerSec,
  };
}

template <typename WorkFn>
ParseBenchmarkResult runWorkBenchmark(std::string_view name,
                                      std::size_t workBytesPerIteration,
                                      WorkFn &&workOnce,
                                      int iterationsOverride = -1,
                                      int warmupOverride = -1) {
  const int iterations =
      iterationsOverride > 0
          ? iterationsOverride
          : getEnvInt("PEGIUM_BENCH_ITERATIONS", 10, /*minValue*/ 1);
  const int warmup = warmupOverride >= 0
                         ? warmupOverride
                         : getEnvInt("PEGIUM_BENCH_WARMUP", 2, /*minValue*/ 0);
  const int samples =
      getEnvInt("PEGIUM_BENCH_SAMPLES", 1, /*minValue*/ 1);

  std::vector<double> throughputs;
  throughputs.reserve(static_cast<std::size_t>(samples));
  std::vector<double> elapsedSamples;
  elapsedSamples.reserve(static_cast<std::size_t>(samples));
  std::size_t processedBytes = 0;

  for (int sample = 0; sample < samples; ++sample) {
    for (int i = 0; i < warmup; ++i) {
      workOnce();
    }

    processedBytes = 0;
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      workOnce();
      processedBytes += workBytesPerIteration;
    }
    const auto end = std::chrono::high_resolution_clock::now();

    const double elapsedMs =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            end - start)
            .count();
    const double elapsedSec = elapsedMs / 1000.0;
    const double mib = static_cast<double>(processedBytes) /
                       static_cast<double>(1024.0 * 1024.0);
    const double mibPerSec = elapsedSec > 0.0 ? (mib / elapsedSec) : 0.0;
    elapsedSamples.push_back(elapsedMs);
    throughputs.push_back(mibPerSec);
  }

  auto sortedThroughputs = throughputs;
  std::ranges::sort(sortedThroughputs);
  const std::size_t medianIndex = sortedThroughputs.size() / 2;
  const double mibPerSec =
      (sortedThroughputs.size() % 2 == 0)
          ? ((sortedThroughputs[medianIndex - 1] + sortedThroughputs[medianIndex]) /
             2.0)
          : sortedThroughputs[medianIndex];

  double elapsedMs = 0.0;
  for (const auto sampleElapsed : elapsedSamples) {
    elapsedMs += sampleElapsed;
  }
  elapsedMs /= static_cast<double>(elapsedSamples.size());

  std::cout << std::fixed << std::setprecision(2)
            << "[microbench] " << name << ": " << mibPerSec << " MiB/s ("
            << processedBytes << " bytes, iterations=" << iterations
            << ", samples=" << samples << ", elapsed=" << elapsedMs
            << " ms avg)\n";

  return ParseBenchmarkResult{
      .bytes = processedBytes,
      .iterations = iterations,
      .elapsed_ms = elapsedMs,
      .mib_per_s = mibPerSec,
  };
}

inline void assertMinThroughput(std::string_view benchmarkKey,
                                double measuredMibPerSec) {
  const auto key = makeBenchmarkKey(benchmarkKey);
  const auto envName = "PEGIUM_BENCH_MIN_MIBPS_" + key;
  const auto minValue = getEnvDouble(envName);
  if (!minValue.has_value()) {
    std::cout << "[microbench] No regression gate for '" << benchmarkKey
              << "'. Set " << envName
              << "=<MiB/s> to enforce a minimum throughput.\n";
    return;
  }
  EXPECT_GE(measuredMibPerSec, *minValue)
      << "Throughput regression for " << benchmarkKey << " (measured "
      << measuredMibPerSec << " MiB/s, required " << *minValue << " MiB/s)";
}

} // namespace pegium::test
