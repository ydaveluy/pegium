#include <requirements/parser/Parser.hpp>
#include <pegium/workspace/Document.hpp>

#include <algorithm>
#include <filesystem>
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

std::string normalized_extension(const std::string &path) {
  std::string ext = std::filesystem::path(path).extension().string();
  std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: pegium-example-requirements-cli <file.req|file.tst>\n";
    return 1;
  }

  try {
    const std::string path = argv[1];
    const auto input = read_text_file(path);
    const auto ext = normalized_extension(path);

    if (ext == ".req") {
      auto parser = std::make_unique<requirements::parser::RequirementsParser>();
      pegium::workspace::Document document;
      document.setText(input);
      parser->parse(document);
      const auto &result = document.parseResult;
      auto *model =
          pegium::ast_ptr_cast<requirements::ast::RequirementModel>(
              result.value);

      if (!result.value || !model) {
        std::cerr << "Parse failed.\n";
        print_parse_diagnostics(result.parseDiagnostics);
        return 2;
      }

      std::cout << "Parsed requirement model with "
                << model->requirements.size() << " requirements.\n";
      return 0;
    }

    if (ext == ".tst") {
      auto parser = std::make_unique<requirements::parser::TestsParser>();
      pegium::workspace::Document document;
      document.setText(input);
      parser->parse(document);
      const auto &result = document.parseResult;
      auto *model =
          pegium::ast_ptr_cast<requirements::ast::TestModel>(result.value);

      if (!result.value || !model) {
        std::cerr << "Parse failed.\n";
        print_parse_diagnostics(result.parseDiagnostics);
        return 2;
      }

      std::cout << "Parsed test model with " << model->tests.size()
                << " tests.\n";
      return 0;
    }

    std::cerr << "Unsupported file extension. Expected .req or .tst\n";
    return 1;
  } catch (const std::exception &error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 3;
  }
}
