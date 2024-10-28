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

template <typename T>
constexpr bool is_data_type_v =
    std::is_same_v<T, bool> || std::is_same_v<T, char> ||
    std::is_same_v<T, std::int8_t> || std::is_same_v<T, std::int16_t> ||
    std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
    std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
    std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t> ||
    std::is_same_v<T, float> || std::is_same_v<T, double> ||
    std::is_same_v<T, std::string> || std::is_enum_v<T>;

enum class DataType {
  Bool,
  Char,
  Int8,
  Int16,
  Int32,
  Int64,
  UInt8,
  UInt16,
  UInt32,
  UInt64,
  Float,
  Double,
  String,
  Enum,
};
template <typename T> struct Reference {

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

  explicit operator bool() const { return get(); }

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

template <typename T> struct is_reference : std::false_type {};

template <typename T> struct is_reference<Reference<T>> : std::true_type {};
template <typename T> constexpr bool is_reference_v = is_reference<T>::value;

template <typename U> static constexpr DataType data_type_of() {
  using T = std::decay_t<U>;
  using enum DataType;
  if constexpr (std::is_same_v<T, bool>)
    return Bool;
  else if constexpr (std::is_same_v<T, char>)
    return Char;
  else if constexpr (std::is_same_v<T, int8_t>)
    return Int8;
  else if constexpr (std::is_same_v<T, int16_t>)
    return Int16;
  else if constexpr (std::is_same_v<T, int32_t>)
    return Int32;
  else if constexpr (std::is_same_v<T, int64_t>)
    return Int64;
  else if constexpr (std::is_same_v<T, uint8_t>)
    return UInt8;
  else if constexpr (std::is_same_v<T, uint16_t>)
    return UInt16;
  else if constexpr (std::is_same_v<T, uint32_t>)
    return UInt32;
  else if constexpr (std::is_same_v<T, uint64_t>)
    return UInt64;
  else if constexpr (std::is_same_v<T, float>)
    return Float;
  else if constexpr (std::is_same_v<T, double>)
    return Double;
  else if constexpr (std::is_enum_v<T>)
    return Enum;
  else if constexpr (std::is_same_v<T, std::string>)
    return String;
  else
    static_assert("Unsupported type");
}

struct AstNode {
  virtual ~AstNode() noexcept = default;
  /// An attribute of type T when T is a data type, and of type
  /// std::shared_ptr<T> otherwise (T must be derived from AstNode).
  /// @tparam T type of the attribute
  template <typename T>
  using attribute =
      std::conditional_t<is_data_type_v<T>, T, std::shared_ptr<T>>;

  /// A reference to an AstNode of type T
  /// @tparam T the AstNode type
  template <typename T> using reference = Reference<T>;

  /// A vector that expand to std::vector<T> when T is a data type or a
  /// reference and to std::vector<std::shared_ptr<T>> otherwise (T must be
  /// derived from AstNode).
  /// @tparam T type of element
  template <typename T>
  using vector =
      std::vector<std::conditional_t<is_reference_v<T>, T, attribute<T>>>;

  // import standard types
  using int8_t = std::int8_t;
  using int16_t = std::int16_t;
  using int32_t = std::int32_t;
  using int64_t = std::int64_t;
  using uint8_t = std::uint8_t;
  using uint16_t = std::uint16_t;
  using uint32_t = std::uint32_t;
  using uint64_t = std::uint64_t;
  using string = std::string;
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
  // grammar rule
  bool hidden = false;
};

struct RootCstNode : public CstNode {
  std::string fullText;
};

} // namespace pegium