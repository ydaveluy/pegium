#include <pegium/core/workspace/DefaultAstNodeLocator.hpp>

#include <algorithm>
#include <charconv>
#include <optional>
#include <stdexcept>

namespace pegium::workspace {

namespace {

struct PathSegment {
  std::string_view propertyName;
  std::optional<std::size_t> propertyIndex;
};

std::optional<PathSegment> parse_segment(std::string_view part) {
  if (part.empty()) {
    return std::nullopt;
  }

  auto propertyName = part;
  std::optional<std::size_t> propertyIndex;
  if (const auto at = part.rfind('@'); at != std::string_view::npos) {
    propertyName = part.substr(0, at);
    std::size_t parsedIndex = 0;
    const auto indexPart = part.substr(at + 1);
    if (const auto parseResult =
            std::from_chars(indexPart.data(),
                            indexPart.data() + indexPart.size(), parsedIndex);
        parseResult.ec != std::errc{} ||
        parseResult.ptr != indexPart.data() + indexPart.size()) {
      return std::nullopt;
    }
    propertyIndex = parsedIndex;
  }

  if (propertyName.empty()) {
    return std::nullopt;
  }

  return PathSegment{.propertyName = propertyName,
                     .propertyIndex = propertyIndex};
}

template <typename NodePtr>
NodePtr descend(NodePtr node, std::string_view path) {
  if (path.empty()) {
    return node;
  }

  std::size_t cursor = 0;
  while (cursor < path.size()) {
    while (cursor < path.size() && path[cursor] == '/') {
      ++cursor;
    }
    if (cursor >= path.size()) {
      break;
    }

    std::size_t end = cursor;
    while (end < path.size() && path[end] != '/') {
      ++end;
    }

    const auto segment = parse_segment(path.substr(cursor, end - cursor));
    if (!segment.has_value()) {
      return nullptr;
    }

    NodePtr selected = nullptr;
    for (NodePtr child : node->getContent()) {
      if (child->getContainerPropertyName() != segment->propertyName ||
          child->getContainerPropertyIndex() != segment->propertyIndex) {
        continue;
      }
      selected = child;
      break;
    }

    if (selected == nullptr) {
      return nullptr;
    }

    node = selected;
    cursor = end;
  }

  return node;
}

[[nodiscard]] std::string_view require_property_name(const AstNode &node) {
  const auto propertyName = node.getContainerPropertyName();
  if (propertyName.empty()) {
    throw std::invalid_argument(
        "AstNodeLocator requires container property metadata for non-root "
        "nodes.");
  }
  return propertyName;
}

} // namespace

std::string DefaultAstNodeLocator::getAstNodePath(const AstNode &node) const {
  std::size_t pathSize = 0;
  for (auto current = &node; current->getContainer() != nullptr;
       current = current->getContainer()) {
    const auto propertyName = require_property_name(*current);
    pathSize += 1 + propertyName.size();
    if (const auto index = current->getContainerPropertyIndex();
        index.has_value()) {
      pathSize += 1 + std::to_string(*index).size();
    }
  }

  std::string path(pathSize, '\0');
  std::size_t cursor = path.size();
  for (auto current = &node; current->getContainer() != nullptr;
       current = current->getContainer()) {
    if (const auto index = current->getContainerPropertyIndex();
        index.has_value()) {
      const auto indexText = std::to_string(*index);
      cursor -= indexText.size();
      std::ranges::copy(indexText, path.begin() + cursor);
      --cursor;
      path[cursor] = '@';
    }

    const auto propertyName = require_property_name(*current);
    cursor -= propertyName.size();
    std::ranges::copy(propertyName, path.begin() + cursor);
    --cursor;
    path[cursor] = '/';
  }
  return path;
}

const AstNode *DefaultAstNodeLocator::getAstNode(const AstNode &root,
                                                 std::string_view path) const {
  return descend(&root, path);
}

AstNode *DefaultAstNodeLocator::getAstNode(AstNode &root,
                                           std::string_view path) const {
  return descend(&root, path);
}

} // namespace pegium::workspace
