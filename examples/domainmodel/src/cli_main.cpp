#include <domainmodel/cli/CliUtils.hpp>
#include <domainmodel/cli/Generator.hpp>
#include <domainmodel/services/Module.hpp>

#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/workspace/Documents.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct GenerateOptions {
  std::string fileName;
  std::optional<std::string> destination;
  std::optional<std::string> root;
  bool quiet = false;
};

std::optional<GenerateOptions> parse_generate_args(int argc, char **argv) {
  if (argc < 3 || std::string_view(argv[1]) != "generate") {
    return std::nullopt;
  }

  GenerateOptions options{.fileName = argv[2]};
  for (int index = 3; index < argc; ++index) {
    const std::string_view arg(argv[index]);
    if ((arg == "-d" || arg == "--destination") && index + 1 < argc) {
      options.destination = std::string(argv[++index]);
      continue;
    }
    if ((arg == "-r" || arg == "--root") && index + 1 < argc) {
      options.root = std::string(argv[++index]);
      continue;
    }
    if (arg == "-q" || arg == "--quiet") {
      options.quiet = true;
      continue;
    }
    return std::nullopt;
  }
  return options;
}

int generate_cli(const GenerateOptions &options) {
  auto shared = pegium::cli::make_shared_services();
  auto services = domainmodel::services::create_language_services(shared);
  auto &domainmodelServices = *services;
  shared.serviceRegistry->registerServices(std::move(services));

  const auto absoluteInputPath = std::filesystem::absolute(options.fileName);
  domainmodel::cli::set_root_folder(
      absoluteInputPath.string(), domainmodelServices,
      options.root.has_value() ? std::optional<std::string_view>(*options.root)
                               : std::nullopt);
  auto document = pegium::cli::build_document_from_path(
      absoluteInputPath.string(), domainmodelServices);

  for (const auto &candidate : shared.workspace.documents->all()) {
    if (pegium::cli::has_error_diagnostics(*candidate)) {
      if (!options.quiet) {
        std::cerr << "There are validation errors:\n";
        pegium::cli::print_error_diagnostics(*candidate, std::cerr);
      }
      return 2;
    }
  }

  const auto &model = domainmodel::cli::extract_ast_node(*document);
  const auto generatedDir = domainmodel::cli::generate_java(
      model, absoluteInputPath.string(),
      options.destination.has_value()
          ? std::optional<std::string_view>(*options.destination)
          : std::nullopt);
  if (!options.quiet) {
    std::cout << "Java classes generated successfully: " << generatedDir << '\n';
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  const auto options = parse_generate_args(argc, argv);
  if (!options.has_value()) {
    std::cerr
        << "Usage: pegium-example-domainmodel-cli generate <file.dmodel> [-d dir] [-r root] [-q]\n";
    return 1;
  }

  try {
    return generate_cli(*options);
  } catch (const std::invalid_argument &error) {
    if (!options->quiet) {
      std::cerr << error.what() << '\n';
    }
    return 1;
  } catch (const std::exception &error) {
    if (!options->quiet) {
      std::cerr << "Fatal error: " << error.what() << '\n';
    }
    return 3;
  }
}
