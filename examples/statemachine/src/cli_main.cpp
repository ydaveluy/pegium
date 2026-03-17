#include <statemachine/parser/Parser.hpp>
#include <pegium/workspace/Document.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::string read_text_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    throw std::runtime_error("Unable to open file: " + path);
  }
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

void print_parse_diagnostics(
    const std::vector<pegium::parser::ParseDiagnostic> &diagnostics) {
  for (const auto &diagnostic : diagnostics) {
    std::cerr << diagnostic << '\n';
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: pegium-example-statemachine-cli <file.statemachine>\n";
    return 1;
  }

  try {
    const auto input = read_text_file(argv[1]);

    auto parser = std::make_unique<statemachine::parser::StateMachineParser>();
    pegium::workspace::Document document;
    document.setText(input);
    parser->parse(document);
    const auto &result = document.parseResult;
    auto *model =
        pegium::ast_ptr_cast<statemachine::ast::Statemachine>(result.value);

    if (!result.value || !model) {
      std::cerr << "Parse failed.\n";
      print_parse_diagnostics(result.parseDiagnostics);
      return 2;
    }

    std::cout << "Parsed statemachine '" << model->name << "' with "
              << model->states.size() << " states.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 3;
  }
}
