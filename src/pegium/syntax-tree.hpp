#pragma once

#include <any>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pegium {

/// A Reference to an AstNode of type T.
/// @tparam T the AstNode type.
template <typename T>
// do not add requirement std::is_base_of_v<AstNode, T> because T may be
// incomplete at this stage.
struct Reference {
  /// Resolve the reference.
  /// @return the resolved reference or nullptr.
  T *get() const {
    if (resolved.load(std::memory_order_acquire)) {
      return ref;
    }
    std::scoped_lock lock(mutex);
    if (!resolved.load(std::memory_order_relaxed)) {
      auto value = resolve(_refText);
      if (value) {
        ref = value.value();
        resolved.store(true, std::memory_order_release);
      }
    }
    return ref;
  }
  T &operator->() { return *get(); }

  /// Check if the reference is resolved
  explicit operator bool() const { return get(); }

  /// Set the text of the reference (internally used by parser)
  /// @param refText the reference text
  /// @return the current object.
  Reference &operator=(std::string refText) noexcept {
    _refText = std::move(refText);
    return *this;
  }

private:
  std::string _refText;
  std::function<std::optional<T *>(const std::string &)> resolve;
  mutable std::atomic_bool resolved = false;
  mutable T *ref = nullptr;
  mutable std::mutex mutex;
};

/// Helpers to check if an object is a Reference
template <typename T> struct is_reference : std::false_type {};

template <typename T> struct is_reference<Reference<T>> : std::true_type {};
template <typename T> constexpr bool is_reference_v = is_reference<T>::value;

/// Represent a node in the AST. Each node in the AST must derived from AstNode
struct AstNode {
  virtual ~AstNode() noexcept = default;

  /// An attribute of type T.
  /// @tparam T the attribute type
  template <typename T> using attribute = T;

  /// A reference to an AstNode of type T.
  /// @tparam T the AstNode type
  template <typename T>
  // do not add requirement std::is_base_of_v<AstNode, T> because T may be
  // incomplete at this stage.
  using reference = Reference<T>;

  /// A containment of an object of type T (T must be derived from AstNode).
  /// @tparam T type of the contained object
  template <typename T>
  // do not add requirement std::is_base_of_v<AstNode, T> because T may be
  // incomplete at this stage.
  using containment = std::shared_ptr<T>;

  /// A vector of elements of type T.
  /// @tparam T type of element

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
  template <typename T> using vector = std::vector<T>;
};

struct RootCstNode;
class GrammarElement;

/**
 * A node in the Concrete Syntax Tree (CST).
 */
struct CstNode {

  /** The container of the node */
  // const CstNode *container;
  /** The actual text */
  std::string_view text;
  /** The root CST node */
  RootCstNode *root;

  /** The AST node created from this CST node */
  // std::any astNode;

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = CstNode;
    using pointer = CstNode *;
    using reference = CstNode &;

    explicit Iterator(pointer root = nullptr);
    reference operator*() const;
    pointer operator->() const;
    Iterator &operator++();
    bool operator==(const Iterator &other) const;
    void prune();

  private:
    struct NodeFrame {
      pointer node;
      size_t childIndex;
      bool prune;
      NodeFrame(pointer n, size_t index, bool p)
          : node(n), childIndex(index), prune(p) {}
      bool operator==(const NodeFrame &other) const {
        return node == other.node /*&& childIndex == other.childIndex &&
               prune == other.prune*/
            ;
      }
    };

    std::vector<std::pair<CstNode *, size_t>> stack;
    bool pruneCurrent = false;

    void advance();
  };

  Iterator begin() { return Iterator(this); }
  Iterator end() { return Iterator(); }

  std::vector<CstNode> content;

  /** The grammar element from which this node was parsed */
  const GrammarElement *grammarSource;
  // A leaf CST node corresponds to a token in the input token stream.
  bool isLeaf = false;
  // Whether the token is hidden, i.e. not explicitly part of the containing
  // grammar rule (e.g: comments)
  bool hidden = false;
};

struct RootCstNode : public CstNode {
  std::string fullText;
};

} // namespace pegium