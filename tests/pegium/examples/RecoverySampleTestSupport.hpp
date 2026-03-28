#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::test {

struct NamedSampleFile {
  std::filesystem::path path;
  std::string label;
  std::string testName;
};

inline void PrintTo(const NamedSampleFile &file, std::ostream *os) {
  *os << file.label;
}

[[nodiscard]] inline std::string read_text_file(
    const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("Unable to read sample file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

[[nodiscard]] inline std::string
sanitize_sample_test_name(std::string_view text) {
  std::string result;
  result.reserve(text.size());

  bool previousWasUnderscore = false;
  for (const unsigned char c : text) {
    if (std::isalnum(c) != 0) {
      result.push_back(static_cast<char>(c));
      previousWasUnderscore = false;
      continue;
    }
    if (!previousWasUnderscore) {
      result.push_back('_');
      previousWasUnderscore = true;
    }
  }

  while (!result.empty() && result.front() == '_') {
    result.erase(result.begin());
  }
  while (!result.empty() && result.back() == '_') {
    result.pop_back();
  }
  if (result.empty()) {
    result = "sample";
  }
  if (std::isdigit(static_cast<unsigned char>(result.front())) != 0) {
    result.insert(0, "sample_");
  }
  return result;
}

[[nodiscard]] inline std::vector<NamedSampleFile> collect_named_sample_files(
    const std::filesystem::path &root,
    std::initializer_list<std::string_view> extensions = {}) {
  std::vector<NamedSampleFile> files;
  if (!std::filesystem::exists(root)) {
    return files;
  }

  std::vector<std::filesystem::path> paths;
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (extensions.size() != 0u) {
      const auto extension = entry.path().extension().generic_string();
      const bool matchesExtension = std::ranges::any_of(
          extensions, [&](std::string_view expected) {
            return extension == expected;
          });
      if (!matchesExtension) {
        continue;
      }
    }
    paths.push_back(entry.path());
  }

  std::ranges::sort(paths, {}, [](const std::filesystem::path &path) {
    return path.generic_string();
  });

  files.reserve(paths.size());
  for (const auto &path : paths) {
    const auto relative = path.lexically_relative(root).generic_string();
    files.push_back({.path = path,
                     .label = relative,
                     .testName = sanitize_sample_test_name(relative)});
  }
  return files;
}

[[nodiscard]] inline std::string dump_parse_diagnostics(
    std::span<const parser::ParseDiagnostic> diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    std::ostringstream current;
    current << diagnostic.kind;
    if (diagnostic.element != nullptr) {
      current << ":" << *diagnostic.element;
    }
    if (!diagnostic.message.empty()) {
      current << ":" << diagnostic.message;
    }
    current << "@" << diagnostic.beginOffset << "-" << diagnostic.endOffset;
    dump += current.str();
  }
  return dump;
}

[[nodiscard]] inline bool has_parse_diagnostic_kind(
    std::span<const parser::ParseDiagnostic> diagnostics,
    parser::ParseDiagnosticKind kind) {
  return std::ranges::any_of(diagnostics, [&](const auto &diagnostic) {
    return diagnostic.kind == kind;
  });
}

struct RecoveryProbeObservation {
  std::string label;
  bool hasValue = false;
  bool fullMatch = false;
  bool recovered = false;
  bool incomplete = false;
  std::size_t parseDiagnosticCount = 0u;
  TextOffset parsedLength = 0u;
  std::string diagnostics;
};

struct RecoveryProbeSummary {
  std::size_t total = 0u;
  std::size_t withValue = 0u;
  std::size_t fullMatch = 0u;
  std::size_t recovered = 0u;
  std::size_t incomplete = 0u;
  std::size_t completeRecovery = 0u;
};

[[nodiscard]] inline bool
is_complete_recovery(const RecoveryProbeObservation &observation) {
  return observation.hasValue && observation.fullMatch &&
         observation.recovered && !observation.incomplete;
}

[[nodiscard]] inline RecoveryProbeObservation observe_recovery_probe(
    std::string label, const workspace::Document &document) {
  const auto &parsed = document.parseResult;
  return {.label = std::move(label),
          .hasValue = parsed.value != nullptr,
          .fullMatch = parsed.fullMatch,
          .recovered = parsed.recoveryReport.hasRecovered,
          .incomplete = has_parse_diagnostic_kind(
              parsed.parseDiagnostics, parser::ParseDiagnosticKind::Incomplete),
          .parseDiagnosticCount = parsed.parseDiagnostics.size(),
          .parsedLength = parsed.parsedLength,
          .diagnostics = dump_parse_diagnostics(parsed.parseDiagnostics)};
}

inline void accumulate_recovery_probe_summary(
    RecoveryProbeSummary &summary,
    const RecoveryProbeObservation &observation) {
  ++summary.total;
  summary.withValue += observation.hasValue ? 1u : 0u;
  summary.fullMatch += observation.fullMatch ? 1u : 0u;
  summary.recovered += observation.recovered ? 1u : 0u;
  summary.incomplete += observation.incomplete ? 1u : 0u;
  summary.completeRecovery += is_complete_recovery(observation) ? 1u : 0u;
}

[[nodiscard]] inline std::string format_recovery_probe_observation(
    const RecoveryProbeObservation &observation) {
  std::ostringstream out;
  out << "PROBE " << observation.label << " value=" << observation.hasValue
      << " full=" << observation.fullMatch
      << " recovered=" << observation.recovered
      << " incomplete=" << observation.incomplete
      << " parsed=" << observation.parsedLength
      << " parseDiag=" << observation.parseDiagnosticCount;
  if (!is_complete_recovery(observation) && !observation.diagnostics.empty()) {
    out << " :: " << observation.diagnostics;
  }
  return out.str();
}

[[nodiscard]] inline std::string format_recovery_probe_summary(
    std::string_view batchName, const RecoveryProbeSummary &summary) {
  std::ostringstream out;
  out << "SUMMARY " << batchName << " total=" << summary.total
      << " value=" << summary.withValue << " full=" << summary.fullMatch
      << " recovered=" << summary.recovered
      << " incomplete=" << summary.incomplete
      << " complete=" << summary.completeRecovery;
  return out.str();
}

template <typename Parser>
[[nodiscard]] inline RecoveryProbeSummary run_recovery_probe_batch(
    Parser &parser, std::span<const NamedSampleFile> samples,
    std::string_view languageId, std::string_view batchName) {
  RecoveryProbeSummary summary;
  for (const auto &sample : samples) {
    const auto text = read_text_file(sample.path);
    auto document = parse_document(parser, text, make_file_uri(sample.label),
                                   std::string(languageId));
    const auto observation = observe_recovery_probe(sample.label, *document);
    accumulate_recovery_probe_summary(summary, observation);
    std::cout << format_recovery_probe_observation(observation) << '\n';
  }

  std::cout << format_recovery_probe_summary(batchName, summary) << '\n';
  return summary;
}

inline void expect_recovery_probe_summary(
    std::string_view batchName, const RecoveryProbeSummary &actual,
    const RecoveryProbeSummary &expected) {
  SCOPED_TRACE(format_recovery_probe_summary(batchName, actual));
  EXPECT_EQ(actual.total, expected.total);
  EXPECT_EQ(actual.withValue, expected.withValue);
  EXPECT_EQ(actual.fullMatch, expected.fullMatch);
  EXPECT_EQ(actual.recovered, expected.recovered);
  EXPECT_EQ(actual.incomplete, expected.incomplete);
  EXPECT_EQ(actual.completeRecovery, expected.completeRecovery);
}

} // namespace pegium::test
