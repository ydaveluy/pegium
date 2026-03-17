#include <pegium/workspace/AstNodeLocator.hpp>

#include <algorithm>
#include <charconv>
#include <optional>

namespace pegium::workspace {

namespace {

struct PathSegment {
  std::string_view propertyName;
  std::optional<std::size_t> propertyIndex;
};

std::optional<PathSegment> parseSegment(std::string_view part) {
  if (part.empty()) {
    return std::nullopt;
  }

  auto propertyName = part;
  std::optional<std::size_t> propertyIndex;
  if (const auto at = part.rfind('@'); at != std::string_view::npos) {
    propertyName = part.substr(0, at);
    std::size_t parsedIndex = 0;
    const auto indexPart = part.substr(at + 1);
    const auto parseResult = std::from_chars(indexPart.data(),
                                             indexPart.data() + indexPart.size(),
                                             parsedIndex);
    if (parseResult.ec != std::errc{} ||
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
  if (node == nullptr || path.empty()) {
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

    const auto part = path.substr(cursor, end - cursor);
    const auto segment = parseSegment(part);
    if (!segment.has_value()) {
      return nullptr;
    }

    NodePtr selected = nullptr;
    for (NodePtr child : node->getContent()) {
      if (child == nullptr ||
          child->getContainerPropertyName() != segment->propertyName) {
        continue;
      }
      if (child->getContainerPropertyIndex() != segment->propertyIndex) {
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

} // namespace

std::string AstNodeLocator::getAstNodePath(const AstNode &node) {
  std::size_t pathSize = 0;
  for (auto current = &node; current->getContainer() != nullptr;
       current = current->getContainer()) {
    if (current->getContainerPropertyName().empty()) {
      return {};
    }

    pathSize += 1 + current->getContainerPropertyName().size();
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
      std::copy(indexText.begin(), indexText.end(),
                path.begin() + static_cast<std::ptrdiff_t>(cursor));
      path[--cursor] = '@';
    }

    const auto propertyName = current->getContainerPropertyName();
    if (propertyName.empty()) {
      return {};
    }
    cursor -= propertyName.size();
    std::copy(propertyName.begin(), propertyName.end(),
              path.begin() + static_cast<std::ptrdiff_t>(cursor));
    path[--cursor] = '/';
  }
  return path;
}

const AstNode *AstNodeLocator::getAstNode(const AstNode &root,
                                          std::string_view path) {
  return descend(&root, path);
}

AstNode *AstNodeLocator::getAstNode(AstNode &root, std::string_view path) {
  return descend(&root, path);
}

} // namespace pegium::workspace
