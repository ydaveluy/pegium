#include <arithmetics/parser/Parser.hpp>
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
    std::cerr << "Usage: pegium-example-arithmetics-cli <file.calc>\n";
    return 1;
  }

  try {
    const auto input = read_text_file(argv[1]);

    auto parser = std::make_unique<arithmetics::parser::ArithmeticParser>();
    pegium::workspace::Document document;
    document.setText(input);
    parser->parse(document);
    auto &result = document.parseResult;

    auto *module = pegium::ast_ptr_cast<arithmetics::ast::Module>(result.value);
    if (!result.value || !module) {
      std::cerr << "Parse failed.\n";
      print_parse_diagnostics(result.parseDiagnostics);
      return 2;
    }

    const auto values = arithmetics::evaluate_module(*module);
    for (std::size_t i = 0; i < values.size(); ++i) {
      std::cout << "[" << i << "] " << values[i] << '\n';
    }

    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 3;
  }
}
