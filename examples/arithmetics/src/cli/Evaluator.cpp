#include "cli/Evaluator.hpp"

#include <arithmetics/parser/Parser.hpp>

#include <iostream>
#include <string_view>

#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/validation/DiagnosticRanges.hpp>

namespace arithmetics::cli {

int eval_file(std::string_view fileName,
              const pegium::CoreServices &services) {
  auto document = pegium::cli::build_document_from_path(fileName, services);
  if (pegium::cli::has_error_diagnostics(*document)) {
    std::cerr << "There are validation errors:\n";
    pegium::cli::print_error_diagnostics(*document, std::cerr);
    return 2;
  }

  auto *module =
      pegium::ast_ptr_cast<arithmetics::ast::Module>(document->parseResult.value);
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

} // namespace arithmetics::cli
