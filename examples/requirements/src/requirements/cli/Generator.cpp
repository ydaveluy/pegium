#include <requirements/cli/Generator.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace requirements::cli {
namespace {

using namespace requirements::ast;

[[nodiscard]] std::string document_source_path(const pegium::AstNode &node) {
  const auto &document = pegium::getDocument(node);
  return pegium::utils::file_uri_to_path(document.uri).value_or(document.uri);
}

} // namespace

std::string generate_summary_file_html_content(
    const ast::RequirementModel &model,
    std::span<const ast::TestModel *const> testModels) {
  std::ostringstream out;
  out << "<html>\n";
  out << "<body>\n";
  out << "<h1>Requirement coverage (demo generator)</h1>\n";
  out << "<div>Source: " << document_source_path(model) << "</div>\n";
  out << "<table border=\"1\">\n";
  out << "<TR><TH>Requirement ID</TH><TH>Testcase ID</TH></TR>\n";
  for (const auto &requirement : model.requirements) {
    if (!requirement) {
      continue;
    }

    out << "<TR><TD>" << requirement->name << "</TD><TD>\n";
    for (const auto *testModel : testModels) {
      if (testModel == nullptr) {
        continue;
      }
      for (const auto &test : testModel->tests) {
        if (!test) {
          continue;
        }
        const bool referencesRequirement = std::ranges::any_of(
            test->requirements, [&](const auto &reference) {
              return reference.get() == requirement.get();
            });
        if (!referencesRequirement) {
          continue;
        }

        out << "<div>" << test->name << " (from "
            << document_source_path(*testModel) << ")<div>\n";
      }
    }
    out << "</TD></TR>\n";
  }
  out << "</table>\n";
  out << "</body>\n";
  out << "</html>\n";
  return out.str();
}

std::string generate_summary(
    const ast::RequirementModel &model,
    std::span<const ast::TestModel *const> testModels, std::string_view filePath,
    std::optional<std::string_view> destination) {
  const auto data = extract_destination_and_name(filePath, destination);
  std::filesystem::create_directories(data.destination);
  const auto outputPath =
      std::filesystem::path(data.destination) / (data.name + ".html");

  std::ofstream out(outputPath, std::ios::binary);
  if (!out.is_open()) {
    throw std::runtime_error("Unable to open output file: " +
                             outputPath.string());
  }
  out << generate_summary_file_html_content(model, testModels);
  return outputPath.string();
}

} // namespace requirements::cli
