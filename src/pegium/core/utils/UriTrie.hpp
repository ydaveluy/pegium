#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pegium/core/utils/UriUtils.hpp>

namespace pegium::utils {

/// Trie keyed by normalized URI path segments.
template <typename T> class UriTrie {
public:
  void clear() { _root.children.clear(); }

  void insert(std::string_view uri, T element) {
    auto *node = getNode(normalize_uri(uri), true);
    node->element = std::move(element);
  }

  void erase(std::string_view uri) {
    auto *node = getNode(normalize_uri(uri), false);
    if (node == nullptr) {
      return;
    }
    if (node == &_root) {
      clear();
      return;
    }
    if (node->parent != nullptr) {
      node->parent->children.erase(node->segment);
    }
  }

  [[nodiscard]] bool has(std::string_view uri) const {
    const auto *node = getNode(normalize_uri(uri), false);
    return node != nullptr && node->element.has_value();
  }

  [[nodiscard]] const T *find(std::string_view uri) const {
    const auto *node = getNode(normalize_uri(uri), false);
    return node != nullptr && node->element.has_value()
               ? std::addressof(*node->element)
               : nullptr;
  }

  [[nodiscard]] T *find(std::string_view uri) {
    auto *node = getNode(normalize_uri(uri), false);
    return node != nullptr && node->element.has_value()
               ? std::addressof(*node->element)
               : nullptr;
  }

  [[nodiscard]] std::vector<T> all() const { return collectValues(_root); }

  [[nodiscard]] std::vector<T> findAll(std::string_view uri) const {
    const auto *node = getNode(normalize_uri(uri), false);
    return node != nullptr ? collectValues(*node) : std::vector<T>{};
  }

private:
  struct Node {
    std::string segment;
    Node *parent = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Node>> children;
    std::optional<T> element;
  };

  [[nodiscard]] static std::vector<std::string>
  splitSegments(std::string_view uri) {
    std::vector<std::string> segments;
    if (uri.empty()) {
      return segments;
    }

    std::size_t begin = 0;
    while (begin <= uri.size()) {
      const auto end = uri.find('/', begin);
      if (end == std::string_view::npos) {
        segments.emplace_back(uri.substr(begin));
        break;
      }
      segments.emplace_back(uri.substr(begin, end - begin));
      begin = end + 1;
      if (begin == uri.size()) {
        break;
      }
    }

    return segments;
  }

  template <typename NodeType>
  [[nodiscard]] static NodeType *getNodeImpl(NodeType &root,
                                             std::string_view normalizedUri,
                                             bool create) {
    auto *current = std::addressof(root);
    for (const auto &segment : splitSegments(normalizedUri)) {
      auto child = current->children.find(segment);
      if (child == current->children.end()) {
        if constexpr (std::is_const_v<NodeType>) {
          return nullptr;
        } else {
          if (!create) {
            return nullptr;
          }
          const auto [inserted, wasInserted] = current->children.try_emplace(
              segment, std::make_unique<Node>(Node{
                           .segment = segment,
                           .parent = current,
                       }));
          (void)wasInserted;
          child = inserted;
        }
      }
      current = child->second.get();
    }
    return current;
  }

  [[nodiscard]] Node *getNode(std::string_view normalizedUri, bool create) {
    return getNodeImpl(_root, normalizedUri, create);
  }

  [[nodiscard]] const Node *getNode(std::string_view normalizedUri,
                                    bool create) const {
    return getNodeImpl(_root, normalizedUri, create);
  }

  [[nodiscard]] static std::vector<T> collectValues(const Node &node) {
    std::vector<T> values;
    if (node.element.has_value()) {
      values.push_back(*node.element);
    }
    for (const auto &[segment, child] : node.children) {
      (void)segment;
      auto childValues = collectValues(*child);
      values.insert(values.end(), std::make_move_iterator(childValues.begin()),
                    std::make_move_iterator(childValues.end()));
    }
    return values;
  }

  Node _root;
};

} // namespace pegium::utils
