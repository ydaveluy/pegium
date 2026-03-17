#pragma once

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
#include <variant>
#include <vector>

#include <pegium/parser/Introspection.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/syntax-tree/Reference.hpp>

namespace pegium {

/// Base type for every AST node produced by pegium parsers.
///
/// `AstNode` stores the structural links of the tree:
/// - direct children
/// - parent/container metadata
/// - the originating CST node when available
///
/// Nodes are intentionally non-copyable and non-movable because container and
/// CST links point into a concrete tree instance.
struct AstNode {
  AstNode() = default;

  /// AST nodes are tree-owned and cannot be moved.
  AstNode(AstNode &&other) = delete;
  /// AST nodes are tree-owned and cannot be copied.
  AstNode(const AstNode &other) = delete;

  /// AST nodes are tree-owned and cannot be move-assigned.
  AstNode &operator=(AstNode &&other) = delete;
  /// AST nodes are tree-owned and cannot be copy-assigned.
  AstNode &operator=(const AstNode &other) = delete;

  /// Virtual destructor for polymorphic AST hierarchies.
  virtual ~AstNode() noexcept = default;

  /// A reference to an AstNode of type T.
  ///
  /// References participate in linking and can resolve across documents.
  template <typename T> using reference = Reference<T>;
  /// A multi-valued reference to AST nodes of type `T`.
  template <typename T> using multi_reference = MultiReference<T>;

  /// A pointer on an object of type T.
  ///
  /// This is the default ownership model for containment relations in generated
  /// AST types.
  template <typename T> using pointer = std::unique_ptr<T>;

  /// Optional value convenience alias used in AST structs.
  template <typename T> using optional = std::optional<T>;
  /// Variant value convenience alias used in AST structs.
  template <typename... T> using variant = std::variant<T...>;

  /// Convenience aliases re-exported for generated AST declarations.
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
  ///
  /// The returned range preserves the stored child order.
  auto getContent() const noexcept {
    return std::views::transform(_content,
                                 [](const AstNode *ptr) { return ptr; });
  }

  /// Returns the direct child at `index`, or `nullptr` when out of bounds.
  [[nodiscard]] const AstNode *getContentAt(std::size_t index) const noexcept {
    return index < _content.size() ? _content[index] : nullptr;
  }

  /// Returns the direct child at `index`, or `nullptr` when out of bounds.
  [[nodiscard]] AstNode *getContentAt(std::size_t index) noexcept {
    return index < _content.size() ? _content[index] : nullptr;
  }

  /// Returns the name of the container property that owns this node.
  ///
  /// The value is empty when the property name was not provided during
  /// container setup.
  [[nodiscard]] std::string_view getContainerPropertyName() const noexcept {
    return _containerPropertyName;
  }

  /// Returns the index within the owning container property when it is a vector.
  ///
  /// Single-valued containment properties return `std::nullopt`.
  [[nodiscard]] std::optional<std::size_t>
  getContainerPropertyIndex() const noexcept {
    if (_containerIndex == std::numeric_limits<std::size_t>::max()) {
      return std::nullopt;
    }
    return _containerIndex;
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
  ///
  /// The current node itself is not included. Traversal order is depth-first
  /// and preserves child order.
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

  /// Returns `true` when this AST node is associated with a CST node.
  [[nodiscard]] bool hasCstNode() const noexcept { return _cstNode.valid(); }

  /// Returns the CST node from which this AST node was parsed.
  ///
  /// Callers should check `hasCstNode()` before assuming the result is valid.
  [[nodiscard]] const CstNodeView &getCstNode() const noexcept {
    return _cstNode;
  }

  /// Returns the CST node from which this AST node was parsed.
  ///
  /// Callers should check `hasCstNode()` before assuming the result is valid.
  [[nodiscard]] CstNodeView &getCstNode() noexcept { return _cstNode; }

  /// Associates this AST node with its originating CST node.
  [[gnu::always_inline]] void setCstNode(const CstNodeView &node) noexcept {
    assert(node.valid());
    assert(!_cstNode.valid());
    _cstNode = node;
  }

  /// Returns the parent node, or nullptr if this is the root.
  const AstNode *getContainer() const noexcept { return _container; }
  /// Returns the parent node, or nullptr if this is the root.
  AstNode *getContainer() noexcept { return _container; }

  /// Returns the nearest ancestor of type `T`, or `nullptr` if none is found.
  ///
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

  /// Returns the nearest ancestor of type `T`, or `nullptr` if none is found.
  ///
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

  /// Sets the container relationship for this node using a compile-time AST feature.
  template <typename Node, auto Feature>
    requires std::derived_from<Node, AstNode> &&
             requires(Node &node) { node.*Feature; }
  void setContainer(
      Node &container,
      std::size_t index = std::numeric_limits<std::size_t>::max()) {
    this->setContainer(container, parser::detail::member_name_v<Feature>, index);
  }

  /// Attaches this node to a container using a compile-time AST feature.
  template <typename Node, auto Feature>
    requires std::derived_from<Node, AstNode> &&
             requires(Node &node) { node.*Feature; }
  void attachToContainer(
      Node &container,
      std::size_t index = std::numeric_limits<std::size_t>::max()) noexcept {
    this->attachToContainer(container, parser::detail::member_name_v<Feature>,
                            index);
  }

  /// Attaches this node to a container without handling reparenting.
  void attachToContainer(
      AstNode &container, std::string_view propertyName,
      std::size_t index = std::numeric_limits<std::size_t>::max()) noexcept {
    assert(_container == nullptr);
    _container = &container;
    _containerPropertyName = propertyName;
    _containerIndex = index;
    _container->_content.push_back(this);
  }

private:
  void setContainer(AstNode &container, std::string_view propertyName,
                    std::size_t index);
  std::vector<AstNode *> _content;
  /// The container node in the AST; every node except the root node has a
  /// container.
  AstNode *_container = nullptr;
  std::string_view _containerPropertyName;
  /// In case the container property is a vector, the element index is stored
  /// here.
  std::size_t _containerIndex = std::numeric_limits<std::size_t>::max();
  /// The Concrete Syntax Tree (CST) node of the text range from which this node
  /// was parsed.
  CstNodeView _cstNode;

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

/// Casts a contained unique pointer to a concrete AST type without transferring ownership.
template <typename T, typename U>
T *ast_ptr_cast(std::unique_ptr<U> &ptr) noexcept {
  return dynamic_cast<T *>(ptr.get());
}

/// Casts a contained unique pointer to a concrete AST type without transferring ownership.
template <typename T, typename U>
const T *ast_ptr_cast(const std::unique_ptr<U> &ptr) noexcept {
  return dynamic_cast<const T *>(ptr.get());
}

} // namespace pegium
