#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <domainmodel/ast.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace pegium::workspace {
class Document;
}

namespace domainmodel::cli {

struct FilePathData {
  std::string destination;
  std::string name;
};

[[nodiscard]] FilePathData extract_destination_and_name(
    std::string_view filePath,
    std::optional<std::string_view> destination);

void set_root_folder(std::string_view fileName,
                     const pegium::services::CoreServices &services,
                     std::optional<std::string_view> root = std::nullopt);

[[nodiscard]] const ast::DomainModel &
extract_ast_node(const pegium::workspace::Document &document);

} // namespace domainmodel::cli
