#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <domainmodel/ast.hpp>
#include <domainmodel/cli/CliUtils.hpp>

namespace domainmodel::cli {

std::string generate_java(const ast::DomainModel &model, std::string_view filePath,
                          std::optional<std::string_view> destination);

} // namespace domainmodel::cli
