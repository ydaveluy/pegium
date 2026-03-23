#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>
#include <domainmodel/parser/Parser.hpp>
#include <requirements/parser/Parser.hpp>
#include <statemachine/parser/Parser.hpp>

#include <pegium/ExampleTestSupport.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::tests {
namespace {

struct ExampleRecoverySpec {
  std::string name;
  std::string uri;
  std::string languageId;
  std::filesystem::path sourcePath;
  std::function<std::shared_ptr<workspace::Document>(std::string)> parse;
};

struct MutationCase {
  std::string label;
  std::string mutated;
  TextOffset offset = 0;
};

enum class PrimitiveEditKind {
  DeleteChar,
  InsertChar,
  ReplaceChar,
  TransposeAdjacent,
};

struct PrimitiveEdit {
  PrimitiveEditKind kind;
  TextOffset offset = 0;
  char value = '\0';
  std::string label;
};

struct SuspiciousRecovery {
  std::string specName;
  std::string mutationLabel;
  TextOffset offset = 0;
  std::string reason;
  bool fullMatch = false;
  TextOffset parsedLength = 0;
  TextOffset maxCursorOffset = 0;
  std::size_t diagnosticCount = 0;
  bool recovered = false;
};

[[nodiscard]] std::filesystem::path repo_root() {
  return pegium::test::current_source_directory()
      .parent_path()
      .parent_path()
      .parent_path();
}

[[nodiscard]] std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path);
  EXPECT_TRUE(input.is_open()) << path.string();
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

[[nodiscard]] bool is_word_char(char c) noexcept {
  const auto uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) != 0 || c == '_';
}

[[nodiscard]] bool is_recoverable_punctuation(char c) noexcept {
  switch (c) {
  case ';':
  case ':':
  case ',':
  case '(':
  case ')':
  case '{':
  case '}':
  case '[':
  case ']':
  case '=':
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '^':
    return true;
  default:
    return false;
  }
}

[[nodiscard]] char replacement_char(char c) noexcept {
  if (std::islower(static_cast<unsigned char>(c)) != 0) {
    return c == 'x' ? 'a' : 'x';
  }
  if (std::isupper(static_cast<unsigned char>(c)) != 0) {
    return c == 'X' ? 'A' : 'X';
  }
  if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
    return c == '9' ? '0' : '9';
  }
  return 'x';
}

struct Span {
  std::size_t begin = 0;
  std::size_t end = 0;
};

[[nodiscard]] std::vector<bool> protected_offsets(std::string_view text) {
  std::vector<bool> protectedBytes(text.size(), false);

  for (std::size_t i = 0; i < text.size();) {
    if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
      const auto begin = i;
      i += 2;
      while (i < text.size() && text[i] != '\n') {
        ++i;
      }
      std::fill(protectedBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                protectedBytes.begin() + static_cast<std::ptrdiff_t>(i), true);
      continue;
    }
    if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
      const auto begin = i;
      i += 2;
      while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
        ++i;
      }
      if (i + 1 < text.size()) {
        i += 2;
      }
      std::fill(protectedBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                protectedBytes.begin() + static_cast<std::ptrdiff_t>(i), true);
      continue;
    }
    if (text[i] == '"' || text[i] == '\'') {
      const auto quote = text[i];
      const auto begin = i++;
      while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
          i += 2;
          continue;
        }
        if (text[i] == quote) {
          ++i;
          break;
        }
        ++i;
      }
      std::fill(protectedBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                protectedBytes.begin() + static_cast<std::ptrdiff_t>(i), true);
      continue;
    }
    ++i;
  }

  return protectedBytes;
}

[[nodiscard]] std::vector<Span>
collect_word_spans(std::string_view text, const std::vector<bool> &protectedBytes) {
  std::vector<Span> spans;
  for (std::size_t i = 0; i < text.size();) {
    if (protectedBytes[i] || !is_word_char(text[i])) {
      ++i;
      continue;
    }
    const auto begin = i;
    while (i < text.size() && !protectedBytes[i] && is_word_char(text[i])) {
      ++i;
    }
    spans.push_back({.begin = begin, .end = i});
  }
  return spans;
}

[[nodiscard]] std::vector<TextOffset>
collect_punctuation_offsets(std::string_view text,
                            const std::vector<bool> &protectedBytes) {
  std::vector<TextOffset> offsets;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (!protectedBytes[i] && is_recoverable_punctuation(text[i])) {
      offsets.push_back(static_cast<TextOffset>(i));
    }
  }
  return offsets;
}

template <typename T>
void shuffle_and_trim(std::vector<T> &values, std::mt19937 &rng,
                      std::size_t maxCount) {
  std::shuffle(values.begin(), values.end(), rng);
  if (values.size() > maxCount) {
    values.resize(maxCount);
  }
}

[[nodiscard]] std::vector<PrimitiveEdit>
generate_primitive_edits(std::string_view text, std::uint32_t seed) {
  std::mt19937 rng(seed);
  const auto protectedBytes = protected_offsets(text);
  auto words = collect_word_spans(text, protectedBytes);
  auto punctuations = collect_punctuation_offsets(text, protectedBytes);

  shuffle_and_trim(words, rng, 14);
  shuffle_and_trim(punctuations, rng, 10);

  std::vector<PrimitiveEdit> edits;
  edits.reserve(words.size() * 4 + punctuations.size() * 2);

  for (const auto &word : words) {
    const auto length = word.end - word.begin;
    if (length == 0) {
      continue;
    }
    const auto mid = word.begin + length / 2;

    if (length > 1) {
      edits.push_back(
          {.kind = PrimitiveEditKind::DeleteChar,
           .offset = static_cast<TextOffset>(mid),
           .label = "delete-word-char@" + std::to_string(word.begin)});
    }

    edits.push_back(
        {.kind = PrimitiveEditKind::InsertChar,
         .offset = static_cast<TextOffset>(mid),
         .value = text[mid],
         .label = "duplicate-word-char@" + std::to_string(word.begin)});

    if (length > 1) {
      const auto transposeIndex = word.begin + (length > 2 ? (length / 2 - 1) : 0);
      edits.push_back(
          {.kind = PrimitiveEditKind::TransposeAdjacent,
           .offset = static_cast<TextOffset>(transposeIndex),
           .label = "transpose-word-char@" + std::to_string(word.begin)});
    }

    edits.push_back(
        {.kind = PrimitiveEditKind::ReplaceChar,
         .offset = static_cast<TextOffset>(mid),
         .value = replacement_char(text[mid]),
         .label = "replace-word-char@" + std::to_string(word.begin)});
  }

  for (const auto offset : punctuations) {
    edits.push_back({.kind = PrimitiveEditKind::DeleteChar,
                     .offset = offset,
                     .label = "delete-punct@" + std::to_string(offset)});

    edits.push_back({.kind = PrimitiveEditKind::InsertChar,
                     .offset = offset,
                     .value = text[offset],
                     .label = "duplicate-punct@" + std::to_string(offset)});
  }

  shuffle_and_trim(edits, rng, 24);
  return edits;
}

void apply_primitive_edit(std::string &text, const PrimitiveEdit &edit) {
  const auto offset = static_cast<std::size_t>(edit.offset);
  if (offset > text.size()) {
    return;
  }
  switch (edit.kind) {
  case PrimitiveEditKind::DeleteChar:
    if (offset < text.size()) {
      text.erase(text.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    break;
  case PrimitiveEditKind::InsertChar:
    text.insert(text.begin() + static_cast<std::ptrdiff_t>(offset), edit.value);
    break;
  case PrimitiveEditKind::ReplaceChar:
    if (offset < text.size()) {
      text[offset] = edit.value;
    }
    break;
  case PrimitiveEditKind::TransposeAdjacent:
    if (offset + 1u < text.size()) {
      std::swap(text[offset], text[offset + 1u]);
    }
    break;
  }
}

[[nodiscard]] MutationCase
build_mutation_case(std::string_view source, const PrimitiveEdit &edit) {
  auto mutated = std::string(source);
  apply_primitive_edit(mutated, edit);
  return MutationCase{
      .label = edit.label,
      .mutated = std::move(mutated),
      .offset = edit.offset,
  };
}

[[nodiscard]] MutationCase
build_mutation_case(std::string_view source, const PrimitiveEdit &first,
                    const PrimitiveEdit &second) {
  auto mutated = std::string(source);
  const PrimitiveEdit *higher = &first;
  const PrimitiveEdit *lower = &second;
  if (higher->offset < lower->offset) {
    std::swap(higher, lower);
  }
  apply_primitive_edit(mutated, *higher);
  apply_primitive_edit(mutated, *lower);
  return MutationCase{
      .label = first.label + "+" + second.label,
      .mutated = std::move(mutated),
      .offset = std::min(first.offset, second.offset),
  };
}

[[nodiscard]] MutationCase
build_mutation_case(std::string_view source, const PrimitiveEdit &first,
                    const PrimitiveEdit &second, const PrimitiveEdit &third) {
  auto mutated = std::string(source);
  std::array<const PrimitiveEdit *, 3> ordered = {&first, &second, &third};
  std::ranges::sort(ordered, [](const PrimitiveEdit *lhs,
                                const PrimitiveEdit *rhs) {
    return lhs->offset > rhs->offset;
  });
  for (const auto *edit : ordered) {
    apply_primitive_edit(mutated, *edit);
  }
  return MutationCase{
      .label = first.label + "+" + second.label + "+" + third.label,
      .mutated = std::move(mutated),
      .offset = std::min({first.offset, second.offset, third.offset}),
  };
}

[[nodiscard]] std::vector<MutationCase>
generate_mutations(std::string_view text, std::uint32_t seed,
                   bool includeClosePairs = false,
                   bool includeCloseTriples = false) {
  std::mt19937 rng(seed);
  auto edits = generate_primitive_edits(text, seed);
  std::vector<MutationCase> mutations;
  mutations.reserve(edits.size() * (includeClosePairs ? 2u : 1u) +
                    (includeCloseTriples ? 12u : 0u));

  for (const auto &edit : edits) {
    mutations.push_back(build_mutation_case(text, edit));
  }

  if (includeClosePairs) {
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    for (std::size_t i = 0; i < edits.size(); ++i) {
      for (std::size_t j = i + 1; j < edits.size(); ++j) {
        const auto lhs = edits[i].offset;
        const auto rhs = edits[j].offset;
        const auto distance = lhs > rhs ? lhs - rhs : rhs - lhs;
        if (distance <= 8u && lhs != rhs) {
          pairs.emplace_back(i, j);
        }
      }
    }
    shuffle_and_trim(pairs, rng, 20);
    for (const auto &[firstIndex, secondIndex] : pairs) {
      mutations.push_back(
          build_mutation_case(text, edits[firstIndex], edits[secondIndex]));
    }
  }

  if (includeCloseTriples) {
    std::vector<std::array<std::size_t, 3>> triples;
    for (std::size_t i = 0; i < edits.size(); ++i) {
      for (std::size_t j = i + 1; j < edits.size(); ++j) {
        for (std::size_t k = j + 1; k < edits.size(); ++k) {
          const auto minOffset =
              std::min({edits[i].offset, edits[j].offset, edits[k].offset});
          const auto maxOffset =
              std::max({edits[i].offset, edits[j].offset, edits[k].offset});
          if (maxOffset - minOffset <= 8u) {
            triples.push_back({i, j, k});
          }
        }
      }
    }
    shuffle_and_trim(triples, rng, 12);
    for (const auto &[firstIndex, secondIndex, thirdIndex] : triples) {
      mutations.push_back(build_mutation_case(text, edits[firstIndex],
                                              edits[secondIndex],
                                              edits[thirdIndex]));
    }
  }

  const auto mutationCap =
      includeCloseTriples ? 48u : (includeClosePairs ? 40u : 24u);
  shuffle_and_trim(mutations, rng, mutationCap);
  return mutations;
}

[[nodiscard]] std::optional<std::string>
classify_suspicious_recovery(const workspace::Document &document,
                             const MutationCase &mutation) {
  const auto textSize =
      static_cast<TextOffset>(document.textDocument().getText().size());
  if (!document.parseResult.fullMatch && document.parseResult.parseDiagnostics.empty()) {
    return "partial parse without syntax diagnostics";
  }
  if (document.parseResult.maxCursorOffset > textSize ||
      document.parseResult.parsedLength > textSize) {
    return "parse offsets escaped the mutated document";
  }
  if (document.parseResult.value == nullptr && document.parseResult.maxCursorOffset == 0u) {
    return "missing AST root without any parsing progress";
  }
  if (document.parseResult.maxCursorOffset <= mutation.offset &&
      mutation.offset + 16u < textSize &&
      document.parseResult.parseDiagnostics.empty()) {
    return "recovery stalled before the mutation without diagnostics";
  }
  return std::nullopt;
}

[[nodiscard]] std::vector<ExampleRecoverySpec> make_example_specs() {
  const auto root = repo_root();

  return {
      {
          .name = "arithmetics/example.calc",
          .uri = pegium::test::make_file_uri("fuzz-example.calc"),
          .languageId = "arithmetics",
          .sourcePath = root / "examples/arithmetics/example/example.calc",
          .parse =
              [](std::string text) {
                arithmetics::parser::ArithmeticParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-example.calc"),
                    "arithmetics");
              },
      },
      {
          .name = "domainmodel/blog.dmodel",
          .uri = pegium::test::make_file_uri("fuzz-blog.dmodel"),
          .languageId = "domain-model",
          .sourcePath = root / "examples/domainmodel/example/blog.dmodel",
          .parse =
              [](std::string text) {
                domainmodel::parser::DomainModelParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-blog.dmodel"),
                    "domain-model");
              },
      },
      {
          .name = "domainmodel/datatypes.dmodel",
          .uri = pegium::test::make_file_uri("fuzz-datatypes.dmodel"),
          .languageId = "domain-model",
          .sourcePath = root / "examples/domainmodel/example/datatypes.dmodel",
          .parse =
              [](std::string text) {
                domainmodel::parser::DomainModelParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-datatypes.dmodel"),
                    "domain-model");
              },
      },
      {
          .name = "domainmodel/qualified-names.dmodel",
          .uri = pegium::test::make_file_uri("fuzz-qualified-names.dmodel"),
          .languageId = "domain-model",
          .sourcePath =
              root / "examples/domainmodel/example/qualified-names.dmodel",
          .parse =
              [](std::string text) {
                domainmodel::parser::DomainModelParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-qualified-names.dmodel"),
                    "domain-model");
              },
      },
      {
          .name = "requirements/requirements.req",
          .uri = pegium::test::make_file_uri("fuzz-requirements.req"),
          .languageId = "requirements-lang",
          .sourcePath = root / "examples/requirements/example/requirements.req",
          .parse =
              [](std::string text) {
                requirements::parser::RequirementsParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-requirements.req"),
                    "requirements-lang");
              },
      },
      {
          .name = "requirements/tests_part1.tst",
          .uri = pegium::test::make_file_uri("fuzz-tests-part1.tst"),
          .languageId = "tests-lang",
          .sourcePath = root / "examples/requirements/example/tests_part1.tst",
          .parse =
              [](std::string text) {
                requirements::parser::TestsParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-tests-part1.tst"),
                    "tests-lang");
              },
      },
      {
          .name = "requirements/tests_part2.tst",
          .uri = pegium::test::make_file_uri("fuzz-tests-part2.tst"),
          .languageId = "tests-lang",
          .sourcePath = root / "examples/requirements/example/tests_part2.tst",
          .parse =
              [](std::string text) {
                requirements::parser::TestsParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-tests-part2.tst"),
                    "tests-lang");
              },
      },
      {
          .name = "statemachine/trafficlight.statemachine",
          .uri = pegium::test::make_file_uri("fuzz-trafficlight.statemachine"),
          .languageId = "statemachine",
          .sourcePath =
              root / "examples/statemachine/example/trafficlight.statemachine",
          .parse =
              [](std::string text) {
                statemachine::parser::StateMachineParser parser;
                return pegium::test::parse_document(
                    parser, std::move(text),
                    pegium::test::make_file_uri("fuzz-trafficlight.statemachine"),
                    "statemachine");
              },
      },
  };
}

TEST(ExampleRecoveryFuzzerTest, SingleEditMutationsPreserveParserSafety) {
  std::vector<SuspiciousRecovery> suspicious;

  for (const auto &spec : make_example_specs()) {
    SCOPED_TRACE(spec.name);
    const auto source = read_file(spec.sourcePath);
    auto baseline = spec.parse(source);
    ASSERT_NE(baseline, nullptr);
    ASSERT_TRUE(baseline->parseSucceeded()) << spec.name;
    ASSERT_NE(baseline->parseResult.value, nullptr) << spec.name;

    const auto mutations = generate_mutations(
        source, static_cast<std::uint32_t>(std::hash<std::string>{}(spec.name)));
    ASSERT_FALSE(mutations.empty()) << spec.name;

    for (const auto &mutation : mutations) {
      auto document = spec.parse(mutation.mutated);
      ASSERT_NE(document, nullptr);

      if (const auto reason = classify_suspicious_recovery(*document, mutation);
          reason.has_value()) {
        suspicious.push_back(SuspiciousRecovery{
            .specName = spec.name,
            .mutationLabel = mutation.label,
            .offset = mutation.offset,
            .reason = *reason,
            .fullMatch = document->parseResult.fullMatch,
            .parsedLength = document->parseResult.parsedLength,
            .maxCursorOffset = document->parseResult.maxCursorOffset,
            .diagnosticCount = document->parseResult.parseDiagnostics.size(),
            .recovered = document->parseResult.recoveryReport.hasRecovered,
        });
      }
    }
  }

  if (!suspicious.empty()) {
    std::ostringstream report;
    report << "Severe recovery safety cases detected:\n";
    const auto count = std::min<std::size_t>(suspicious.size(), 12u);
    for (std::size_t index = 0; index < count; ++index) {
      const auto &entry = suspicious[index];
      report << " - " << entry.specName << " [" << entry.mutationLabel
             << "] offset=" << entry.offset << ": " << entry.reason
             << " fullMatch=" << entry.fullMatch
             << " parsedLength=" << entry.parsedLength
             << " maxCursorOffset=" << entry.maxCursorOffset
             << " diagnostics=" << entry.diagnosticCount
             << " recovered=" << entry.recovered
             << "\n";
    }
    ADD_FAILURE() << report.str();
  }
}

TEST(ExampleRecoveryFuzzerTest, CloseDoubleMutationsPreserveParserSafety) {
  std::vector<SuspiciousRecovery> suspicious;

  for (const auto &spec : make_example_specs()) {
    SCOPED_TRACE(spec.name);
    const auto source = read_file(spec.sourcePath);
    auto baseline = spec.parse(source);
    ASSERT_NE(baseline, nullptr);
    ASSERT_TRUE(baseline->parseSucceeded()) << spec.name;
    ASSERT_NE(baseline->parseResult.value, nullptr) << spec.name;

    const auto mutations = generate_mutations(
        source,
        static_cast<std::uint32_t>(std::hash<std::string>{}(spec.name) ^ 0x9e3779b9u),
        true);
    ASSERT_FALSE(mutations.empty()) << spec.name;

    for (const auto &mutation : mutations) {
      auto document = spec.parse(mutation.mutated);
      ASSERT_NE(document, nullptr);

      if (const auto reason = classify_suspicious_recovery(*document, mutation);
          reason.has_value()) {
        suspicious.push_back(SuspiciousRecovery{
            .specName = spec.name,
            .mutationLabel = mutation.label,
            .offset = mutation.offset,
            .reason = *reason,
            .fullMatch = document->parseResult.fullMatch,
            .parsedLength = document->parseResult.parsedLength,
            .maxCursorOffset = document->parseResult.maxCursorOffset,
            .diagnosticCount = document->parseResult.parseDiagnostics.size(),
            .recovered = document->parseResult.recoveryReport.hasRecovered,
        });
      }
    }
  }

  if (!suspicious.empty()) {
    std::ostringstream report;
    report << "Severe close-double recovery safety cases detected:\n";
    const auto count = std::min<std::size_t>(suspicious.size(), 12u);
    for (std::size_t index = 0; index < count; ++index) {
      const auto &entry = suspicious[index];
      report << " - " << entry.specName << " [" << entry.mutationLabel
             << "] offset=" << entry.offset << ": " << entry.reason
             << " fullMatch=" << entry.fullMatch
             << " parsedLength=" << entry.parsedLength
             << " maxCursorOffset=" << entry.maxCursorOffset
             << " diagnostics=" << entry.diagnosticCount
             << " recovered=" << entry.recovered
             << "\n";
    }
    ADD_FAILURE() << report.str();
  }
}

TEST(ExampleRecoveryFuzzerTest, CloseTripleMutationsPreserveParserSafety) {
  std::vector<SuspiciousRecovery> suspicious;

  for (const auto &spec : make_example_specs()) {
    SCOPED_TRACE(spec.name);
    const auto source = read_file(spec.sourcePath);
    auto baseline = spec.parse(source);
    ASSERT_NE(baseline, nullptr);
    ASSERT_TRUE(baseline->parseSucceeded()) << spec.name;
    ASSERT_NE(baseline->parseResult.value, nullptr) << spec.name;

    const auto mutations = generate_mutations(
        source,
        static_cast<std::uint32_t>(std::hash<std::string>{}(spec.name) ^
                                   0x85ebca6bu),
        true, true);
    ASSERT_FALSE(mutations.empty()) << spec.name;

    for (const auto &mutation : mutations) {
      auto document = spec.parse(mutation.mutated);
      ASSERT_NE(document, nullptr);

      if (const auto reason = classify_suspicious_recovery(*document, mutation);
          reason.has_value()) {
        suspicious.push_back(SuspiciousRecovery{
            .specName = spec.name,
            .mutationLabel = mutation.label,
            .offset = mutation.offset,
            .reason = *reason,
            .fullMatch = document->parseResult.fullMatch,
            .parsedLength = document->parseResult.parsedLength,
            .maxCursorOffset = document->parseResult.maxCursorOffset,
            .diagnosticCount = document->parseResult.parseDiagnostics.size(),
            .recovered = document->parseResult.recoveryReport.hasRecovered,
        });
      }
    }
  }

  if (!suspicious.empty()) {
    std::ostringstream report;
    report << "Severe close-triple recovery safety cases detected:\n";
    const auto count = std::min<std::size_t>(suspicious.size(), 12u);
    for (std::size_t index = 0; index < count; ++index) {
      const auto &entry = suspicious[index];
      report << " - " << entry.specName << " [" << entry.mutationLabel
             << "] offset=" << entry.offset << ": " << entry.reason
             << " fullMatch=" << entry.fullMatch
             << " parsedLength=" << entry.parsedLength
             << " maxCursorOffset=" << entry.maxCursorOffset
             << " diagnostics=" << entry.diagnosticCount
             << " recovered=" << entry.recovered
             << "\n";
    }
    ADD_FAILURE() << report.str();
  }
}

} // namespace
} // namespace pegium::tests
