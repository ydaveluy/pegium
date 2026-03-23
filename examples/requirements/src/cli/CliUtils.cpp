#include <requirements/cli/CliUtils.hpp>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include <pegium/cli/CliUtils.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/syntax-tree/AstUtils.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/Document.hpp>
#include <pegium/core/workspace/DocumentBuilder.hpp>
#include <pegium/core/workspace/Documents.hpp>
#include <pegium/core/workspace/WorkspaceManager.hpp>
#include <pegium/core/workspace/WorkspaceProtocol.hpp>

namespace requirements::cli {
namespace {

[[nodiscard]] std::string sanitize_file_stem(std::string_view filePath) {
  auto stem = std::filesystem::path(filePath).stem().string();
  const auto [newEnd, end] =
      std::ranges::remove_if(stem, [](char ch) { return ch == '.' || ch == '-'; });
  stem.erase(newEnd, end);
  return stem;
}

} // namespace

FilePathData extract_destination_and_name(
    std::string_view filePath,
    std::optional<std::string_view> destination) {
  return {
      .destination =
          destination.has_value() ? std::string(*destination) : std::string("generated"),
      .name = sanitize_file_stem(filePath),
  };
}

ExtractedRequirementModelWithTests extract_requirement_model_with_test_models(
    std::string_view fileName,
    const pegium::services::CoreServices &services) {
  const auto absoluteFilePath = std::filesystem::absolute(std::string(fileName));
  const auto workspaceRoot = absoluteFilePath.parent_path();
  const std::vector<pegium::workspace::WorkspaceFolder> workspaceFolders{{
      .uri = pegium::utils::path_to_file_uri(workspaceRoot.string()),
      .name = "main",
  }};
  services.shared.workspace.workspaceManager->initializeWorkspace(
      workspaceFolders);

  std::vector<std::shared_ptr<pegium::workspace::Document>> documents;
  for (const auto &document : services.shared.workspace.documents->all()) {
    documents.push_back(document);
  }

  pegium::workspace::BuildOptions buildOptions;
  buildOptions.validation = true;
  services.shared.workspace.documentBuilder->build(documents, buildOptions);

  auto mainDocument = pegium::cli::build_document_from_path(fileName, services);
  const auto hasMainDocument =
      std::ranges::any_of(documents, [&mainDocument](const auto &document) {
        return document->uri == mainDocument->uri;
      });
  if (!hasMainDocument) {
    documents.push_back(mainDocument);
  }

  const auto *requirementModel =
      pegium::ast_ptr_cast<ast::RequirementModel>(mainDocument->parseResult.value);
  if (requirementModel == nullptr) {
    throw std::runtime_error("Unable to generate requirement coverage for invalid model.");
  }

  std::vector<const ast::TestModel *> testModels;
  for (const auto &document : documents) {
    if (const auto *testModel =
            pegium::ast_ptr_cast<ast::TestModel>(document->parseResult.value);
        testModel != nullptr) {
      testModels.push_back(testModel);
    }
  }

  return {
      .mainDocument = std::move(mainDocument),
      .documents = std::move(documents),
      .requirementModel = requirementModel,
      .testModels = std::move(testModels),
  };
}

} // namespace requirements::cli
