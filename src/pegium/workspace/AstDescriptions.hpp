#pragma once

#include <optional>
#include <string>
#include <variant>

#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/workspace/Symbol.hpp>

namespace pegium::workspace {

struct LinkingError {
  ReferenceInfo info;
  std::string message;
  std::optional<AstNodeDescription> targetDescription;
};

using AstNodeDescriptionOrError = std::variant<AstNodeDescription, LinkingError>;
using AstNodeDescriptionsOrError =
    std::variant<std::vector<AstNodeDescription>, LinkingError>;

} // namespace pegium::workspace
