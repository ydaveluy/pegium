#include <domainmodel/cli/CliUtils.hpp>

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
#include <pegium/core/workspace/WorkspaceManager.hpp>
#include <pegium/core/workspace/WorkspaceProtocol.hpp>

namespace domainmodel::cli {
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

void set_root_folder(std::string_view fileName,
                     const pegium::services::CoreServices &services,
                     std::optional<std::string_view> root) {
  auto rootPath = std::filesystem::path(std::string(fileName)).parent_path();
  if (root.has_value()) {
    rootPath = std::filesystem::path(*root);
  }
  if (!rootPath.is_absolute()) {
    rootPath = std::filesystem::absolute(rootPath);
  }

  const std::vector<pegium::workspace::WorkspaceFolder> workspaceFolders{{
      .uri = pegium::utils::path_to_file_uri(rootPath.string()),
      .name = rootPath.filename().string(),
  }};
  services.shared.workspace.workspaceManager->initializeWorkspace(
      workspaceFolders);
}

const ast::DomainModel &
extract_ast_node(const pegium::workspace::Document &document) {
  const auto *model =
      pegium::ast_ptr_cast<ast::DomainModel>(document.parseResult.value);
  if (model == nullptr) {
    throw std::runtime_error("Unable to generate Java for invalid domain model.");
  }
  return *model;
}

} // namespace domainmodel::cli
