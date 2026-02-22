#pragma once

#include <any>
#include <atomic>
#include <cassert>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
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
    if (__builtin_expect(_resolved.load(std::memory_order_acquire), true)) {
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
  T *operator->() {
    T *ptr = get();
    assert(ptr);
    return ptr;
  }
  const T *operator->() const {
    const T *ptr = get();
    assert(ptr);
    return ptr;
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
      : container{container}, property{feature}, index{npos},
        _isInstance{makeIsInstance<T>()} {
    assert(dynamic_cast<Class *>(container));

    auto *typedContainer = static_cast<Class *>(container);
    _installResolver =
        [typedContainer, feature](
            const std::function<AstNode *(const std::string &)> &resolver) {
          (typedContainer->*feature)._resolver = resolver;
        };
  }
  template <typename T, typename Class>
  ReferenceInfo(AstNode *container, std::vector<Reference<T>> Class::*feature,
                std::size_t index)
      : container{container}, property{feature}, index{index},
        _isInstance{makeIsInstance<T>()} {
    assert(dynamic_cast<Class *>(container));

    auto *typedContainer = static_cast<Class *>(container);
    _installResolver =
        [typedContainer, feature,
         index](const std::function<AstNode *(const std::string &)> &resolver) {
          (typedContainer->*feature)[index]._resolver = resolver;
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
  void installResolver(
      const std::function<AstNode *(const std::string &)> &resolver) const {
    _installResolver(resolver);
  }

  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

private:
  AstNode *container;
  std::any property;
  std::size_t index;
  std::function<bool(const AstNode *)> _isInstance;
  std::function<void(const std::function<AstNode *(const std::string &)> &)>
      _installResolver;
  template <typename T> static constexpr auto makeIsInstance() noexcept {
    return [](const AstNode *node) {
      return dynamic_cast<const T *>(node) != nullptr;
    };
  }
};

} // namespace pegium
