#pragma once

#include <any>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <pegium/grammar/AbstractElement.hpp>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace pegium {

struct AstNode;

/// A Reference of type T.
/// @tparam T the AstNode type.
template <typename T> struct Reference {
  Reference() = default;
  /// Create a reference with a reference text. (used by parser to initialize a
  /// reference)
  /// @param refText the reference text
  explicit Reference(std::string refText) noexcept
      : _refText{std::move(refText)} {}

  Reference(const Reference &) = delete;
  Reference &operator=(const Reference &) = delete;
  Reference(Reference &&other) noexcept
      : _refText(std::move(other._refText)),
        _resolver(std::move(other._resolver)),
        _resolved(other._resolved.load()), _ref(other._ref) {}

  Reference &operator=(Reference &&other) noexcept {
    if (this != &other) {
      std::scoped_lock lock(_mutex, other._mutex);
      _refText = std::move(other._refText);
      _resolver = std::move(other._resolver);
      _resolved.store(other._resolved.load());
      _ref = other._ref;
    }
    return *this;
  }

  /// Resolve the reference.
  /// @return the resolved reference or nullptr.
  T *get() const {
    if (_resolved.load(std::memory_order_acquire)) {
      return _ref;
    }
    std::scoped_lock lock(_mutex);
    if (!_resolved.load(std::memory_order_relaxed)) {
      assert(_resolver &&
             "The resolver must be installed before accessing the reference.");
      // TODO probably safe to use static_cast here ?
      _ref = dynamic_cast<T *>(_resolver(_refText));
      _resolved.store(true, std::memory_order_release);
    }
    return _ref;
  }
  T &operator->() {
    T *ptr = get();
    assert(ptr);
    return *ptr;
  }

  /// Check if the reference is resolved
  explicit operator bool() const { return get(); }

private:
  std::string _refText;
  std::function<AstNode *(const std::string &)> _resolver;
  mutable std::atomic_bool _resolved = false;
  mutable T *_ref = nullptr;
  mutable std::mutex _mutex;
  friend class ReferenceInfo;
};

/// Helpers to check if an object is a Reference
template <typename T> struct is_reference : std::false_type {};
template <typename T> struct is_reference<Reference<T>> : std::true_type {};
template <typename T>
struct is_reference<std::vector<Reference<T>>> : std::true_type {};
template <typename T> constexpr bool is_reference_v = is_reference<T>::value;

struct ReferenceInfo {

  template <typename T, typename Class>
  ReferenceInfo(AstNode *container, Reference<T> Class::*feature)
      : container{container}, property{feature}, index{npos} {
    assert(dynamic_cast<Class *>(container));

    _isInstance = makeIsInstance<T>();
    auto *typedContainer = static_cast<Class *>(container);
    _installResolver =
        [typedContainer,
         feature](std::function<AstNode *(const std::string &)> resolver) {
          (typedContainer->*feature)._resolver = std::move(resolver);
        };
  }
  template <typename T, typename Class>
  ReferenceInfo(AstNode *container, std::vector<Reference<T>> Class::*feature,
                std::size_t index)
      : container{container}, property{feature}, index{index} {
    assert(dynamic_cast<Class *>(container));
    _isInstance = makeIsInstance<T>();

    auto *typedContainer = static_cast<Class *>(container);
    _installResolver =
        [typedContainer, feature,
         index](std::function<AstNode *(const std::string &)> resolver) {
          (typedContainer->*feature)[index]._resolver = std::move(resolver);
        };
  }
  ReferenceInfo(ReferenceInfo &&) noexcept = default;
  ReferenceInfo(const ReferenceInfo &) = default;
  ReferenceInfo &operator=(ReferenceInfo &&) noexcept = default;
  ReferenceInfo &operator=(const ReferenceInfo &) = default;
  ReferenceInfo() = default;

  bool isInstance(const AstNode *node) const noexcept {
    return _isInstance(node);
  }
  void installResolver(std::function<AstNode *(const std::string &)> resolver) {
    _installResolver(std::move(resolver));
  }

  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

private:
  AstNode *container;
  std::any property;
  std::size_t index;
  std::function<bool(const AstNode *)> _isInstance;
  std::function<void(std::function<AstNode *(const std::string &)>)>
      _installResolver;
  template <typename T> static constexpr auto makeIsInstance() noexcept {
    return [](const AstNode *node) {
      return dynamic_cast<const T *>(node) != nullptr;
    };
  }
};

struct CstNode;
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
  auto getContent() { return std::views::all(_content); }
  /// Returns the direct children of this node.
  auto getContent() const {
    return std::views::transform(
        _content, [](const AstNode *ptr) -> const AstNode * { return ptr; });
  }

  /// Returns the direct children of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getContent() const {
    return of_type<T>(getContent());
  }
  /// Returns the direct children of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getContent() {
    return of_type<T>(getContent());
  }

  /// Returns all descendants of this node.
  auto getAllContent() { return Range<AstNode *>(this); }

  /// Returns all descendants of this node.
  auto getAllContent() const { return Range<const AstNode *>(this); }

  /// Returns all descendants of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getAllContent() const {
    return of_type<T>(getAllContent());
  }

  /// Returns all descendants of type T.
  template <typename T>
    requires std::derived_from<T, AstNode>
  auto getAllContent() {
    return of_type<T>(getAllContent());
  }

  /// Returns the references of this node.
  auto getReferences() { return std::views::all(_references); }
  /// Returns the references of this node.
  auto getReferences() const {
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
  std::size_t _containerIndex;
  /// The Concrete Syntax Tree (CST) node of the text range from which this node
  /// was parsed.
  CstNode *_node;

  template <typename T, typename Range> static auto of_type(Range &&range) {
    using Ptr = std::ranges::range_value_t<Range>;
    return std::forward<Range>(range) | std::views::filter([](Ptr ptr) {
             return dynamic_cast<std::conditional_t<
                        std::is_const_v<std::remove_pointer_t<Ptr>>, const T *,
                        T *>>(ptr) != nullptr;
           }) |
           std::views::transform([](Ptr ptr) {
             return static_cast<std::conditional_t<
                 std::is_const_v<std::remove_pointer_t<Ptr>>, const T *, T *>>(
                 ptr);
           });
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

struct RootCstNode;

/**
 * A node in the Concrete Syntax Tree (CST).
 */
struct CstNode {
  /// The actual text */
  std::string_view text;
  /// The grammar element from which this node was parsed
  const grammar::AbstractElement *grammarSource;

  std::vector<CstNode> content;

  /// A leaf CST node corresponds to a token in the input token stream.
  bool isLeaf() const noexcept { return content.empty(); }
  /// Whether the token is hidden, i.e. not explicitly part of the containing
  /// grammar rule (e.g: comments)
  bool hidden = false;

private:
  friend std::ostream &operator<<(std::ostream &os, const CstNode &obj);
};

struct RootCstNode : public CstNode {
  std::string fullText;
};

} // namespace pegium