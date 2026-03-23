#pragma once

#include <cassert>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include <pegium/core/syntax-tree/AbstractReference.hpp>

namespace pegium {

/// Typed single-valued AST reference.
template <typename T> struct Reference : AbstractSingleReference {
private:
  static constexpr std::string_view kIncompatibleTargetMessage =
      "Incompatible reference target type: resolved node cannot be cast "
      "to the expected reference type.";

public:
  Reference() noexcept = default;

  void clearLinkStateUnlocked() const noexcept override {
    resetBaseLinkStateUnlocked();
    _target = nullptr;
    _description.reset();
    _errorMessage.clear();
  }

  [[nodiscard]] const AstNode *resolve() const override {
    return get();
  }

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

  [[nodiscard]] std::string_view getErrorMessage() const noexcept override {
    return _errorMessage;
  }

protected:
  [[nodiscard]] const workspace::AstNodeDescription &
  resolvedDescription() const override {
    ensureResolved();
    assert(_description.has_value());
    return *_description;
  }

private:
  void ensureResolved() const {
    using enum ReferenceState;
    auto state = _state;
    if (state == Resolving) {
      throw CyclicReferenceResolution(*this);
    }
    if (state == Resolved || state == Error) {
      return;
    }

    if (_linker == nullptr) {
      _target = nullptr;
      _description.reset();
      _errorMessage = "No linker is available for this reference.";
      _state = Error;
      return;
    }

    _target = nullptr;
    _description.reset();
    _errorMessage.clear();
    _state = Resolving;

    const auto resolution = _linker->resolve(*this);

    _target = nullptr;
    _description.reset();
    if (const auto *error = std::get_if<workspace::LinkingError>(&resolution);
        error != nullptr) {
      if (error->retryable) {
        _state = Unresolved;
        return;
      }
      _errorMessage = error->message;
      _state = Error;
      return;
    }

    auto resolved = std::get<workspace::ResolvedAstNodeDescription>(resolution);
    const auto *typedNode = dynamic_cast<const T *>(resolved.node);
    if (typedNode == nullptr) {
      _errorMessage = kIncompatibleTargetMessage;
      _state = Error;
      return;
    }

    _target = typedNode;
    _description = std::move(resolved.description);
    _state = ReferenceState::Resolved;
  }

  mutable const T *_target = nullptr;
  mutable std::optional<workspace::AstNodeDescription> _description;
  mutable std::string _errorMessage;
};

/// Typed multi-valued AST reference.
template <typename T> struct MultiReference : AbstractMultiReference {
private:
  static constexpr std::string_view kIncompatibleTargetMessage =
      "Incompatible reference target type: resolved node cannot be cast "
      "to the expected reference type.";

public:
  MultiReference() noexcept = default;

  void clearLinkStateUnlocked() const noexcept override {
    resetBaseLinkStateUnlocked();
    _descriptions.clear();
    _targets.clear();
    _errorMessage.clear();
  }

  [[nodiscard]] std::span<const T *const> resolveAll() const {
    ensureResolved();
    return std::span<const T *const>(_targets.data(), _targets.size());
  }

  [[nodiscard]] const T *operator[](std::size_t index) const {
    ensureResolved();
    return _targets[index];
  }

  [[nodiscard]] const T *front() const {
    ensureResolved();
    assert(!_targets.empty());
    return _targets.front();
  }

  [[nodiscard]] const T *back() const {
    ensureResolved();
    assert(!_targets.empty());
    return _targets.back();
  }

  [[nodiscard]] const T *const *data() const {
    ensureResolved();
    return _targets.data();
  }

  [[nodiscard]] auto begin() const {
    ensureResolved();
    return _targets.begin();
  }

  [[nodiscard]] auto end() const {
    ensureResolved();
    return _targets.end();
  }

  [[nodiscard]] auto cbegin() const {
    ensureResolved();
    return _targets.cbegin();
  }

  [[nodiscard]] auto cend() const {
    ensureResolved();
    return _targets.cend();
  }

  [[nodiscard]] std::size_t size() const {
    ensureResolved();
    return _targets.size();
  }

  [[nodiscard]] bool empty() const {
    ensureResolved();
    return _targets.empty();
  }

  explicit operator bool() const { return !empty(); }

  [[nodiscard]] std::string_view getErrorMessage() const noexcept override {
    return _errorMessage;
  }

protected:
  [[nodiscard]] std::size_t resolvedDescriptionCount() const override {
    ensureResolved();
    return _descriptions.size();
  }

  [[nodiscard]] const workspace::AstNodeDescription &
  resolvedDescriptionAt(std::size_t index) const override {
    ensureResolved();
    assert(index < _descriptions.size());
    return _descriptions[index];
  }

private:
  void ensureResolved() const {
    using enum ReferenceState;
    auto state = _state;
    if (state == Resolving) {
      throw CyclicReferenceResolution(*this);
    }
    if (state == Resolved || state == Error) {
      return;
    }

    if (_linker == nullptr) {
      _descriptions.clear();
      _targets.clear();
      _errorMessage = "No linker is available for this reference.";
      _state = Error;
      return;
    }

    _descriptions.clear();
    _targets.clear();
    _errorMessage.clear();
    _state = Resolving;

    const auto resolution = _linker->resolveAll(*this);

    _descriptions.clear();
    _targets.clear();
    if (const auto *error = std::get_if<workspace::LinkingError>(&resolution);
        error != nullptr) {
      if (error->retryable) {
        _state = Unresolved;
        return;
      }
      _errorMessage = error->message;
      _state = Error;
      return;
    }

    auto descriptions =
        std::get<std::vector<workspace::ResolvedAstNodeDescription>>(resolution);
    _descriptions.reserve(descriptions.size());
    _targets.reserve(descriptions.size());
    for (auto &resolved : descriptions) {
      const auto *typedNode = dynamic_cast<const T *>(resolved.node);
      if (typedNode == nullptr) {
        _descriptions.clear();
        _targets.clear();
        _errorMessage = kIncompatibleTargetMessage;
        _state = Error;
        return;
      }
      _descriptions.push_back(std::move(resolved.description));
      _targets.push_back(typedNode);
    }
    _state = ReferenceState::Resolved;
  }

  mutable std::vector<workspace::AstNodeDescription> _descriptions;
  mutable std::vector<const T *> _targets;
  mutable std::string _errorMessage;
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
