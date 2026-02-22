#pragma once

#include <any>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <pegium/syntax-tree/Reference.hpp>

namespace pegium {

/// Represent a node in the AST. Each node in the AST must derives from AstNode
struct AstNode {
  AstNode() = default;

  // delete Copy/Move constructor
  AstNode(AstNode &&other) = delete;
  AstNode(const AstNode &other) = delete;

  // delete Copy/Move assignment
  AstNode &operator=(AstNode &&other) = delete;
  AstNode &operator=(const AstNode &other) = delete;

  // Destructeur
  virtual ~AstNode() noexcept = default;

  /// A reference to an AstNode of type T.
  /// @tparam T the AstNode type
  template <typename T> using reference = Reference<T>;

  /// A pointer on an object of type T.
  /// @tparam T type of the contained object
  template <typename T> using pointer = std::shared_ptr<T>;

  template <typename T> using optional = std::optional<T>;

  // Import standard types for convenience
  using int8_t = std::int8_t;
  using int16_t = std::int16_t;
  using int32_t = std::int32_t;
  using int64_t = std::int64_t;
  using uint8_t = std::uint8_t;
  using uint16_t = std::uint16_t;
  using uint32_t = std::uint32_t;
  using uint64_t = std::uint64_t;
  using string = std::string;

  /// A vector of elements of type T.
  /// @tparam T type of element
  template <typename T> using vector = std::vector<T>;

  /// Returns the direct children of this node.
  auto getContent() noexcept { return std::views::all(_content); }
  /// Returns the direct children of this node.
  auto getContent() const noexcept {
    return std::views::transform(_content,
                                 [](const AstNode *ptr) { return ptr; });
  }

  /// Returns the direct children of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getContent() const noexcept {
    return of_type<T>(getContent());
  }
  /// Returns the direct children of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getContent() noexcept {
    return of_type<T>(getContent());
  }

  /// Returns all descendants of this node.
  auto getAllContent() noexcept { return Range<AstNode *>(this); }

  /// Returns all descendants of this node.
  auto getAllContent() const noexcept { return Range<const AstNode *>(this); }

  /// Returns all descendants of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getAllContent() const noexcept {
    return of_type<T>(getAllContent());
  }

  /// Returns all descendants of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getAllContent() noexcept {
    return of_type<T>(getAllContent());
  }

  /// Returns the references of this node.
  auto getReferences() noexcept { return std::views::all(_references); }
  /// Returns the references of this node.
  auto getReferences() const noexcept {
    return std::views::transform(
        _references,
        [](const ReferenceInfo &ref) -> const ReferenceInfo & { return ref; });
  }

  template <typename T, typename Class>
  void addReference(Reference<T> Class::*feature) {
    _references.emplace_back(this, feature);
  }
  template <typename T, typename Class>
  void addReference(std::vector<Reference<T>> Class::*feature,
                    std::size_t index) {
    _references.emplace_back(this, feature, index);
  }

  /// Returns the parent node, or nullptr if this is the root.
  const AstNode *getContainer() const noexcept;
  /// Returns the parent node, or nullptr if this is the root.
  AstNode *getContainer() noexcept;

  /// Returns the nearest ancestor of type T, or nullptr if none is found.
  /// If the current node itself matches, it is returned.
  template <typename T>
    requires std::derived_from<T, AstNode>
  const T *getContainer() const noexcept {
    const auto *item = this;
    do {
      if (const auto *casted = dynamic_cast<const T *>(item)) {
        return casted;
      }
      item = item->getContainer();
    } while (item);
    return nullptr;
  }

  /// Returns the nearest ancestor of type T, or nullptr if none is found.
  /// If the current node itself matches, it is returned.
  template <typename T>
    requires std::derived_from<T, AstNode>
  T *getContainer() noexcept {
    auto *item = this;
    do {
      if (auto *casted = dynamic_cast<T *>(item)) {
        return casted;
      }
      item = item->getContainer();
    } while (item);
    return nullptr;
  }

  template <typename Node, typename Base, typename T>
    requires std::derived_from<Node, AstNode> && std::derived_from<Node, Base>
  void
  setContainer(Node *container, T Base::*property,
               std::size_t index = std::numeric_limits<std::size_t>::max()) {
    this->setContainer(container, std::any(property), index);
  }

private:
  void setContainer(AstNode *container, std::any property, std::size_t index);
  std::vector<AstNode *> _content;
  std::vector<ReferenceInfo> _references;
  /// The container node in the AST; every node except the root node has a
  /// container.
  AstNode *_container = nullptr;
  /// The property of the `_container` node that contains this node. This is
  /// either a direct reference or a vector.
  std::any _containerProperty;
  /// In case `_containerProperty` is a vector, the vector index is stored here.
  std::size_t _containerIndex = std::numeric_limits<std::size_t>::max();
  /// The Concrete Syntax Tree (CST) node of the text range from which this node
  /// was parsed.
  // CstNode *_node = nullptr;

  template <typename T, typename Range>
  static auto of_type(Range &&range) noexcept {
    using Ptr = std::ranges::range_value_t<Range>;
    using CastedPtr =
        std::conditional_t<std::is_const_v<std::remove_pointer_t<Ptr>>,
                           const T *, T *>;
    return std::forward<Range>(range) |
           std::views::filter([](Ptr ptr) noexcept {
             return dynamic_cast<CastedPtr>(ptr) != nullptr;
           }) |
           std::views::transform(
               [](Ptr ptr) noexcept { return static_cast<CastedPtr>(ptr); });
  }
  template <typename NodePtr> class Iterator {
  public:
    using value_type = NodePtr;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;
    using reference = NodePtr;
    using pointer = void;
    Iterator() = default;
    explicit Iterator(NodePtr root) {
      if (root) {
        for (auto it = root->_content.rbegin(); it != root->_content.rend();
             ++it) {
          stack.push_back(*it);
        }
      }
    }
    reference operator*() const { return stack.back(); }
    Iterator &operator++() {
      NodePtr current = stack.back();
      stack.pop_back();
      for (auto it = current->_content.rbegin(); it != current->_content.rend();
           ++it) {
        assert((*it)->getContainer() == current);
        stack.push_back(*it);
      }
      return *this;
    }
    Iterator operator++(int) {
      Iterator temp = *this;
      ++(*this);
      return temp;
    }
    bool operator==(const Iterator &other) const = default;

  private:
    std::vector<NodePtr> stack;
  };
  template <typename NodePtr>
  class Range : public std::ranges::view_interface<Range<NodePtr>> {
  public:
    using iterator = Iterator<NodePtr>;
    explicit Range(NodePtr root) : root_(root) {}
    iterator begin() const { return iterator(root_); }
    iterator end() const { return iterator(); }

  private:
    NodePtr root_;
  };
};

} // namespace pegium
