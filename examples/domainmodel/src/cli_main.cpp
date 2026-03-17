#include <domainmodel/parser/Parser.hpp>
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
    std::cerr << "Usage: pegium-example-domainmodel-cli <file.dmodel>\n";
    return 1;
  }

  try {
    const auto input = read_text_file(argv[1]);

    auto parser = std::make_unique<domainmodel::parser::DomainModelParser>();
    pegium::workspace::Document document;
    document.setText(input);
    parser->parse(document);
    const auto &result = document.parseResult;

    auto *model =
        pegium::ast_ptr_cast<domainmodel::ast::DomainModel>(result.value);
    if (!result.value || !model) {
      std::cerr << "Parse failed.\n";
      print_parse_diagnostics(result.parseDiagnostics);
      return 2;
    }

    std::cout << "Parsed domain model with " << model->elements.size()
              << " top-level elements.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 3;
  }
}
