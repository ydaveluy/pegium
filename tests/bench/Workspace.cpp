#include "BenchmarkSupport.hpp"

#include <arithmetics/core/Module.hpp>
#include <domainmodel/core/Module.hpp>
#include <requirements/core/Module.hpp>
#include <statemachine/core/Module.hpp>

// Per-language workspace benchmarks: build many self-contained files of one
// language simultaneously at startup (a small ~250 KB workspace and a large
// ~12 MB one), like the fastbelt / language-tool-benchmark setup. Each file is a
// complete, valid program with unique top-level names so the workspace has no
// duplicate-symbol diagnostics. The generators produce byte-identical input
// to the external comparison bench harness.
namespace pegium::bench {
namespace {

using FileGenerator = std::string (*)(std::size_t fileIndex,
                                      std::size_t perFileBytes);

constexpr std::size_t kPerFileBytes = 8 * 1024;

std::string arithmetics_file(std::size_t fileIndex, std::size_t perFileBytes) {
  std::string source = "module Bench" + std::to_string(fileIndex) + "\n\n";
  std::size_t index = 0;
  while (source.size() < perFileBytes) {
    if (index == 0) {
      source += "def value0: 1 + 2;\n";
    } else {
      source += "def value" + std::to_string(index) + ": value" +
                std::to_string(index - 1) + " + 1;\n";
    }
    if (index % 16 == 0) {
      source += "value" + std::to_string(index) + ";\n";
    }
    ++index;
  }
  return source;
}

std::string domainmodel_file(std::size_t fileIndex, std::size_t perFileBytes) {
  const auto suffix = std::to_string(fileIndex);
  std::string source = "datatype String" + suffix + "\n";
  source += "package bench" + suffix + " {\n";
  std::size_t index = 0;
  while (source.size() < perFileBytes) {
    source += "  entity Entity" + std::to_string(index);
    if (index > 0) {
      source += " extends Entity" + std::to_string(index - 1);
    }
    source += " {\n    name: String" + suffix + "\n";
    if (index > 0) {
      source += "    prev: Entity" + std::to_string(index - 1) + "\n";
    }
    source += "  }\n";
    ++index;
  }
  source += "}\n";
  return source;
}

std::string requirements_file(std::size_t fileIndex, std::size_t perFileBytes) {
  constexpr std::size_t kEnvironmentCount = 24;
  const auto suffix = std::to_string(fileIndex);
  std::string source = "contact: \"bench\"\n";
  for (std::size_t index = 0; index < kEnvironmentCount; ++index) {
    source += "environment E" + suffix + "_" + std::to_string(index) +
              ": \"Environment " + std::to_string(index) + "\"\n";
  }
  std::size_t requirementIndex = 0;
  while (source.size() < perFileBytes) {
    source += "req R" + suffix + "_" + std::to_string(requirementIndex) +
              " \"Requirement " + std::to_string(requirementIndex) +
              "\" applicable for E" + suffix + "_" +
              std::to_string(requirementIndex % kEnvironmentCount) + ", E" +
              suffix + "_" +
              std::to_string((requirementIndex + 1) % kEnvironmentCount) + "\n";
    ++requirementIndex;
  }
  return source;
}

std::string statemachine_file(std::size_t fileIndex, std::size_t perFileBytes) {
  constexpr std::size_t kEventCount = 64;
  constexpr std::size_t kCommandCount = 64;
  std::string source = "statemachine Bench" + std::to_string(fileIndex) + "\n";
  source += "events";
  for (std::size_t index = 0; index < kEventCount; ++index) {
    source += " Event" + std::to_string(index);
  }
  source += "\ncommands";
  for (std::size_t index = 0; index < kCommandCount; ++index) {
    source += " Command" + std::to_string(index);
  }
  source += "\ninitialState State0\n";
  std::size_t stateIndex = 0;
  while (source.size() < perFileBytes) {
    source += "state State" + std::to_string(stateIndex) + " actions { Command" +
              std::to_string(stateIndex % kCommandCount) + " }\n";
    source += "Event" + std::to_string(stateIndex % kEventCount) + " => State" +
              std::to_string(stateIndex + 1) + "\n";
    source += "end\n";
    ++stateIndex;
  }
  source += "state State" + std::to_string(stateIndex) +
            " actions { Command0 }\n";
  source += "Event0 => State0\nend\n";
  return source;
}

std::vector<std::string> generate_files(FileGenerator fileGen,
                                        std::size_t targetBytes) {
  std::vector<std::string> files;
  std::size_t total = 0;
  std::size_t index = 0;
  while (total < targetBytes) {
    auto text = fileGen(index, kPerFileBytes);
    total += text.size();
    files.push_back(std::move(text));
    ++index;
  }
  return files;
}

BenchmarkTimings
measure_workspace_iteration(bool (*registerLanguages)(SharedCoreServices &),
                            const std::string &languageId,
                            const std::string &extension,
                            const std::vector<std::string> &files) {
  auto shared = make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  if (!registerLanguages(*shared)) {
    throw std::runtime_error("Failed to register services for " + languageId);
  }

  const auto textDocuments = shared->lsp.textDocuments;
  std::vector<std::shared_ptr<workspace::Document>> documents;
  documents.reserve(files.size());
  for (std::size_t index = 0; index < files.size(); ++index) {
    const auto uri = utils::path_to_file_uri(
        "/tmp/pegium-bench/ws/" + languageId + "/" + std::to_string(index) +
        extension);
    auto textDocument = std::make_shared<workspace::TextDocument>(
        workspace::TextDocument::create(uri, languageId, 1, files[index]));
    (void)textDocuments->set(textDocument);
    const auto stored = textDocuments->get(uri);
    if (stored == nullptr) {
      throw std::runtime_error("Failed to open workspace document.");
    }
    auto document = std::make_shared<workspace::Document>(stored, uri);
    shared->workspace.documents->addDocument(document);
    documents.push_back(std::move(document));
  }

  // Hand the whole document set to the framework's DocumentBuilder and time the
  // full build — it parallelizes each phase across the workspace internally.
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  workspace::BuildOptions options;
  options.validation = true;
  shared->workspace.documentBuilder->build(documents, options);
  const auto end = Clock::now();

  for (const auto &document : documents) {
    if (document->state != workspace::DocumentState::Validated) {
      throw std::runtime_error("Workspace build did not reach Validated.");
    }
  }

  BenchmarkTimings timings{};
  timings[static_cast<std::size_t>(BenchmarkStep::FullBuild)] =
      std::chrono::duration<double, std::milli>(end - start).count();
  return timings;
}

void register_language_workspaces(BenchmarkRegistry &registry,
                                  const std::string &name,
                                  const std::string &languageId,
                                  const std::string &extension,
                                  bool (*registerLanguages)(SharedCoreServices &),
                                  FileGenerator fileGen) {
  const std::array<std::pair<std::string, std::size_t>, 2> cases{
      {{"small", get_env_size("PEGIUM_BENCH_WS_SMALL", 256 * 1024, 16 * 1024)},
       {"large", get_env_size("PEGIUM_BENCH_WS_LARGE", 12 * 1024 * 1024,
                              16 * 1024)}}};

  const auto filter = get_env_string("PEGIUM_BENCH_FILTER");
  for (const auto &[suffix, target] : cases) {
    const std::string benchName = name + "-workspace-" + suffix;
    // Skip generating (and holding) the workspaces the filter excludes, so a
    // single-config run's peak RSS reflects only that workspace.
    if (!filter.empty() && benchName.find(filter) == std::string::npos) {
      continue;
    }
    auto files = generate_files(fileGen, target);
    std::size_t bytes = 0;
    for (const auto &file : files) {
      bytes += file.size();
    }
    registry.add(
        name + "-workspace-" + suffix + " files=" + std::to_string(files.size()),
        bytes,
        [registerLanguages, languageId, extension,
         files = std::move(files)] {
          return measure_workspace_iteration(registerLanguages, languageId,
                                              extension, files);
        },
        /*fullBuildOnly=*/true);
  }
}

} // namespace

void register_workspace_benchmarks(BenchmarkRegistry &registry) {
  register_language_workspaces(registry, "arithmetics", "arithmetics", ".calc",
                               arithmetics::registerArithmeticsServices,
                               arithmetics_file);
  register_language_workspaces(registry, "domainmodel", "domain-model",
                               ".dmodel",
                               domainmodel::registerDomainModelServices,
                               domainmodel_file);
  register_language_workspaces(registry, "requirements", "requirements-lang",
                               ".req",
                               requirements::registerRequirementsServices,
                               requirements_file);
  register_language_workspaces(registry, "statemachine", "statemachine",
                               ".statemachine",
                               statemachine::registerStatemachineServices,
                               statemachine_file);
}

} // namespace pegium::bench
