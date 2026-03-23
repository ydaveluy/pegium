#include <arithmetics/parser/Parser.hpp>
#include <arithmetics/services/Module.hpp>

#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/validation/DiagnosticRanges.hpp>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct EvalOptions {
  std::string fileName;
};

std::optional<EvalOptions> parse_eval_args(int argc, char **argv) {
  if (argc != 3 || std::string_view(argv[1]) != "eval") {
    return std::nullopt;
  }
  return EvalOptions{.fileName = argv[2]};
}

int eval_cli(const EvalOptions &options) {
  auto shared = pegium::cli::make_shared_services();
  auto services = arithmetics::services::create_language_services(shared);
  auto &arithmeticsServices = *services;
  shared.serviceRegistry->registerServices(std::move(services));

  auto document = pegium::cli::build_document_from_path(
      options.fileName, arithmeticsServices);
  if (pegium::cli::has_error_diagnostics(*document)) {
    std::cerr << "There are validation errors:\n";
    pegium::cli::print_error_diagnostics(*document, std::cerr);
    return 2;
  }

  auto *module = pegium::ast_ptr_cast<arithmetics::ast::Module>(document->parseResult.value);
  if (module == nullptr) {
    std::cerr << "Unable to evaluate invalid arithmetics module.\n";
    return 2;
  }

  const auto results = arithmetics::interpret_evaluations(*module);
  for (const auto &statement : module->statements) {
    const auto *evaluation =
        dynamic_cast<const arithmetics::ast::Evaluation *>(statement.get());
    if (evaluation == nullptr || evaluation->expression == nullptr) {
      continue;
    }

    const auto result = results.find(evaluation);
    if (result == results.end()) {
      continue;
    }

    const auto [begin, end] =
        pegium::validation::range_of(*evaluation->expression);
    const auto &textDocument = document->textDocument();
    const auto position = textDocument.positionAt(begin);
    const auto text = textDocument.getText().substr(begin, end - begin);
    std::cout << "line " << position.line + 1 << ": " << text << " ===> "
              << result->second << '\n';
  }

  return 0;
}

} // namespace

int main(int argc, char **argv) {
  const auto options = parse_eval_args(argc, argv);
  if (!options.has_value()) {
    std::cerr << "Usage: pegium-example-arithmetics-cli eval <file.calc>\n";
    return 1;
  }

  try {
    return eval_cli(*options);
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 3;
  }
}
