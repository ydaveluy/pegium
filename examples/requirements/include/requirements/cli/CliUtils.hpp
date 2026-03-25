#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <requirements/ast.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace pegium::workspace {
class Document;
}

namespace requirements::cli {

struct FilePathData {
  std::string destination;
  std::string name;
};

struct ExtractedRequirementModelWithTests {
  std::shared_ptr<pegium::workspace::Document> mainDocument;
  std::vector<std::shared_ptr<pegium::workspace::Document>> documents;
  const ast::RequirementModel *requirementModel = nullptr;
  std::vector<const ast::TestModel *> testModels;
};

[[nodiscard]] FilePathData extract_destination_and_name(
    std::string_view filePath,
    std::optional<std::string_view> destination);

[[nodiscard]] ExtractedRequirementModelWithTests
extract_requirement_model_with_test_models(
    std::string_view fileName, const pegium::CoreServices &services);

} // namespace requirements::cli
