#pragma once

#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include <pegium/core/syntax-tree/AbstractReference.hpp>

namespace pegium {

/// Typed single-valued AST reference.
template <typename T> struct Reference : AbstractSingleReference {
public:
  Reference() noexcept = default;

  void clearLinkState() const noexcept override {
    _target = nullptr;
    _description = nullptr;
    resetBaseLinkState();
  }

  [[nodiscard]] const AstNode *resolve() const override { return get(); }

  [[nodiscard]] const T *get() const {
    ensureResolved();
    return _target;
  }

  const T *operator->() const {
    const auto *ptr = get();
    assert(ptr != nullptr);
    return ptr;
  }

  explicit operator bool() const { return get() != nullptr; }

protected:
  [[nodiscard]] const workspace::AstNodeDescription &
  resolvedDescription() const override {
    ensureResolved();
    assert(_description != nullptr);
    return *_description;
  }

private:
  void ensureResolved() const {
    if (!acquireResolverRole()) {
      return;
    }

    if (_linker == nullptr) {
      publishState(ReferenceState::ErrorNoLinker);
      return;
    }

    const auto resolution = _linker->resolve(*this);

    if (const auto *error = std::get_if<workspace::LinkingError>(&resolution);
        error != nullptr) {
      applyLinkingError(error->kind);
      return;
    }

    const auto &resolved =
        std::get<workspace::ResolvedAstNodeDescription>(resolution);
    // The scope provider already type-checked candidates against the
    // reference's expected type via `AstReflection::isSubtype` (see
    // `DefaultScopeProvider::find_scope_entry`). A correct linker therefore
    // hands us a node that IS-A `T`, making the cast a static downcast.
    assert(dynamic_cast<const T *>(resolved.node) != nullptr);
    _target = static_cast<const T *>(resolved.node);
    _description = resolved.description;
    publishState(ReferenceState::Resolved);
  }

  mutable const T *_target = nullptr;
  mutable const workspace::AstNodeDescription *_description = nullptr;
};

/// Resolved entry of a multi-valued reference: pairs the (non-owning) node
/// description pointer with the typed pointer to the resolved AST node.
///
/// Pointer lifetimes mirror those of `Reference<T>`: descriptions live in the
/// owning document's index, AST nodes live in the parsed document.
template <typename T> struct MultiReferenceItem {
  const workspace::AstNodeDescription *description = nullptr;
  const T *ref = nullptr;
};

/// Typed multi-valued AST reference.
template <typename T> struct MultiReference : AbstractMultiReference {
public:
  using Item = MultiReferenceItem<T>;

  MultiReference() noexcept = default;

  void clearLinkState() const noexcept override {
    _items.clear();
    resetBaseLinkState();
  }

  [[nodiscard]] std::span<const Item> items() const {
    ensureResolved();
    return std::span<const Item>(_items.data(), _items.size());
  }

  [[nodiscard]] std::span<const Item> resolveAll() const { return items(); }

  [[nodiscard]] const T *operator[](std::size_t index) const {
    ensureResolved();
    return _items[index].ref;
  }

  [[nodiscard]] const T *front() const {
    ensureResolved();
    assert(!_items.empty());
    return _items.front().ref;
  }

  [[nodiscard]] const T *back() const {
    ensureResolved();
    assert(!_items.empty());
    return _items.back().ref;
  }

  [[nodiscard]] auto begin() const {
    ensureResolved();
    return _items.begin();
  }

  [[nodiscard]] auto end() const {
    ensureResolved();
    return _items.end();
  }

  [[nodiscard]] auto cbegin() const {
    ensureResolved();
    return _items.cbegin();
  }

  [[nodiscard]] auto cend() const {
    ensureResolved();
    return _items.cend();
  }

  [[nodiscard]] std::size_t size() const {
    ensureResolved();
    return _items.size();
  }

  [[nodiscard]] bool empty() const {
    ensureResolved();
    return _items.empty();
  }

  explicit operator bool() const { return !empty(); }

protected:
  [[nodiscard]] std::size_t resolvedDescriptionCount() const override {
    ensureResolved();
    return _items.size();
  }

  [[nodiscard]] const workspace::AstNodeDescription &
  resolvedDescriptionAt(std::size_t index) const override {
    ensureResolved();
    assert(index < _items.size());
    assert(_items[index].description != nullptr);
    return *_items[index].description;
  }

private:
  void ensureResolved() const {
    if (!acquireResolverRole()) {
      return;
    }

    if (_linker == nullptr) {
      publishState(ReferenceState::ErrorNoLinker);
      return;
    }

    const auto resolution = _linker->resolveAll(*this);

    if (const auto *error = std::get_if<workspace::LinkingError>(&resolution);
        error != nullptr) {
      applyLinkingError(error->kind);
      return;
    }

    const auto &descriptions =
        std::get<std::vector<workspace::ResolvedAstNodeDescription>>(resolution);
    _items.reserve(descriptions.size());
    for (const auto &resolved : descriptions) {
      assert(dynamic_cast<const T *>(resolved.node) != nullptr);
      _items.push_back(Item{.description = resolved.description,
                            .ref = static_cast<const T *>(resolved.node)});
    }
    publishState(ReferenceState::Resolved);
  }

  mutable std::vector<Item> _items;
};

template <typename T> struct is_reference : std::false_type {};
template <typename T> struct is_reference<Reference<T>> : std::true_type {};
template <typename T> struct is_reference<MultiReference<T>> : std::true_type {};
template <typename T>
struct is_reference<std::optional<Reference<T>>> : std::true_type {};
template <typename T>
struct is_reference<std::optional<MultiReference<T>>> : std::true_type {};
template <typename T>
struct is_reference<std::vector<Reference<T>>> : std::true_type {};
template <typename T>
struct is_reference<std::vector<MultiReference<T>>> : std::true_type {};
template <typename T>
inline constexpr bool is_reference_v = is_reference<T>::value;

template <typename T> struct is_multi_reference : std::false_type {};
template <typename T>
struct is_multi_reference<MultiReference<T>> : std::true_type {};
template <typename T>
struct is_multi_reference<std::optional<MultiReference<T>>> : std::true_type {};
template <typename T>
struct is_multi_reference<std::vector<MultiReference<T>>> : std::true_type {};
template <typename T>
inline constexpr bool is_multi_reference_v = is_multi_reference<T>::value;

} // namespace pegium
