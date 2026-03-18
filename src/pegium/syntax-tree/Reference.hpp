#pragma once

#include <atomic>
#include <cassert>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/workspace/Symbol.hpp>

namespace pegium {

struct AstNode;

namespace references {
class Linker;
}

/// Result of resolving a single-valued reference.
struct ReferenceResolution {
  AstNode *node = nullptr;
  const workspace::AstNodeDescription *description = nullptr;
  std::string errorMessage;
};

/// Result of resolving a multi-valued reference.
struct MultiReferenceResolution {
  std::vector<ReferenceResolution> items;
  std::string errorMessage;
};

class AbstractReference;

/// Lightweight contextual information about one reference slot on an AST node.
struct ReferenceInfo {
  const AbstractReference *reference = nullptr;
  AstNode *container = nullptr;
  std::string_view property;
  std::optional<std::size_t> index;

  /// Returns `true` when this structure points to an actual reference object.
  [[nodiscard]] explicit operator bool() const noexcept {
    return reference != nullptr;
  }
};

/// Non-owning handle to a concrete reference stored inside an AST object.
///
/// `ReferenceHandle` lets generic services enumerate references without knowing
/// whether they are stored directly, inside an optional, or inside a vector.
struct ReferenceHandle {
  using Getter = AbstractReference *(*)(void *, std::size_t) noexcept;
  using ConstGetter =
      const AbstractReference *(*)(const void *, std::size_t) noexcept;

  void *owner = nullptr;
  Getter getter = nullptr;
  ConstGetter constGetter = nullptr;
  std::size_t index = 0;

  /// Returns the mutable reference object, or `nullptr` when unavailable.
  [[nodiscard]] AbstractReference *get() const noexcept {
    return getter == nullptr ? nullptr : getter(owner, index);
  }

  /// Returns the const reference object, or `nullptr` when unavailable.
  [[nodiscard]] const AbstractReference *getConst() const noexcept {
    return constGetter == nullptr ? nullptr : constGetter(owner, index);
  }

  /// Returns `true` when the handle can currently resolve to a reference object.
  [[nodiscard]] explicit operator bool() const noexcept {
    return getConst() != nullptr;
  }

  /// Creates a handle for a reference stored directly by address.
  template <typename Ref>
  static ReferenceHandle direct(Ref *reference) noexcept {
    static_assert(std::derived_from<Ref, AbstractReference>);
    return ReferenceHandle{
        .owner = reference,
        .getter = [](void *owner, std::size_t) noexcept -> AbstractReference * {
          return static_cast<Ref *>(owner);
        },
        .constGetter =
            [](const void *owner,
               std::size_t) noexcept -> const AbstractReference * {
          return static_cast<const Ref *>(owner);
        },
        .index = 0,
    };
  }

  /// Creates a handle for a reference stored inside an indexable collection.
  template <typename RefVector>
  static ReferenceHandle indexed(RefVector *references,
                                 std::size_t index) noexcept {
    using Ref = typename RefVector::value_type;
    static_assert(std::derived_from<Ref, AbstractReference>);
    return ReferenceHandle{
        .owner = references,
        .getter = [](void *owner, std::size_t index) noexcept
            -> AbstractReference * {
          auto *items = static_cast<RefVector *>(owner);
          return items != nullptr && index < items->size() ? &(*items)[index]
                                                           : nullptr;
        },
        .constGetter =
            [](const void *owner,
               std::size_t index) noexcept -> const AbstractReference * {
          const auto *items = static_cast<const RefVector *>(owner);
          return items != nullptr && index < items->size() ? &(*items)[index]
                                                           : nullptr;
        },
        .index = index,
    };
  }

  /// Creates a handle for a reference stored inside an optional-like wrapper.
  template <typename RefOptional>
  static ReferenceHandle optional(RefOptional *reference) noexcept {
    using Ref = typename RefOptional::value_type;
    static_assert(std::derived_from<Ref, AbstractReference>);
    return ReferenceHandle{
        .owner = reference,
        .getter = [](void *owner, std::size_t) noexcept -> AbstractReference * {
          auto *item = static_cast<RefOptional *>(owner);
          return item != nullptr && item->has_value() ? &item->value() : nullptr;
        },
        .constGetter =
            [](const void *owner,
               std::size_t) noexcept -> const AbstractReference * {
          const auto *item = static_cast<const RefOptional *>(owner);
          return item != nullptr && item->has_value() ? &item->value() : nullptr;
        },
        .index = 0,
    };
  }
};

namespace references {
/// Resolves one single-valued reference through `linker`.
ReferenceResolution resolveReference(const Linker &linker,
                                     const AbstractReference &reference);
/// Resolves every target of a multi-valued reference through `linker`.
MultiReferenceResolution resolveAllReferences(const Linker &linker,
                                              const AbstractReference &reference);
}

/// Current resolution state of a reference.
enum class ReferenceState : std::uint8_t {
  Unresolved,
  Resolving,
  Resolved,
  Error,
};

/// Untyped base class for single and multi references stored on AST nodes.
///
/// References keep enough metadata for generic services:
/// - the owning AST node
/// - the referenced text and optional CST node
/// - the expected target runtime type
/// - the linker used for lazy resolution
class AbstractReference {
public:
  AbstractReference() = default;
  explicit AbstractReference(bool multi) noexcept : _isMulti(multi) {}

  AbstractReference(const AbstractReference &) = delete;
  AbstractReference &operator=(const AbstractReference &) = delete;

  AbstractReference(AbstractReference &&other) noexcept
      : _container(other._container), _referenceType(other._referenceType),
        _accepts(other._accepts), _property(other._property),
        _propertyIndex(other._propertyIndex),
        _refText(std::move(other._refText)), _refNode(other._refNode),
        _linker(other._linker),
        _isMulti(other._isMulti),
        _state(other._state.load(std::memory_order_acquire)),
        _targets(std::move(other._targets)),
        _errorMessage(std::move(other._errorMessage)) {}

  AbstractReference &operator=(AbstractReference &&other) noexcept {
    if (this != &other) {
      std::scoped_lock lock(_mutex, other._mutex);
      _container = other._container;
      _referenceType = other._referenceType;
      _accepts = other._accepts;
      _property = other._property;
      _propertyIndex = other._propertyIndex;
      _refText = std::move(other._refText);
      _refNode = other._refNode;
      _linker = other._linker;
      _isMulti = other._isMulti;
      _state.store(other._state.load(std::memory_order_relaxed),
                   std::memory_order_release);
      _targets = std::move(other._targets);
      _errorMessage = std::move(other._errorMessage);
    }
    return *this;
  }

  [[nodiscard]] AstNode *getContainer() const noexcept { return _container; }

  /// Returns the expected runtime type of resolved targets.
  [[nodiscard]] std::type_index getReferenceType() const noexcept {
    return _referenceType;
  }

  /// Returns the name of the AST property that owns this reference.
  [[nodiscard]] std::string_view getProperty() const noexcept {
    return _property;
  }

  /// Returns the index inside the owning property when it is multi-valued.
  [[nodiscard]] std::optional<std::size_t> getPropertyIndex() const noexcept {
    if (_propertyIndex == kNoPropertyIndex) {
      return std::nullopt;
    }
    return _propertyIndex;
  }

  /// Returns whether `node` is accepted as a compatible target.
  [[nodiscard]] bool accepts(const AstNode *node) const noexcept {
    return _accepts != nullptr && _accepts(node);
  }

  /// Returns `true` for multi-valued references.
  [[nodiscard]] bool isMulti() const noexcept { return _isMulti; }

  /// Returns the CST node that produced the reference text, when available.
  [[nodiscard]] std::optional<CstNodeView> getRefNode() const noexcept {
    if (_refNode.valid()) {
      return _refNode;
    }
    return std::nullopt;
  }

  /// Updates the originating CST node and invalidates cached resolution.
  void setRefNode(const CstNodeView &node) noexcept {
    std::scoped_lock lock(_mutex);
    _refNode = node;
    clearLinkState();
  }

  /// Returns the raw source text used to resolve this reference.
  [[nodiscard]] const std::string &getRefText() const noexcept {
    return _refText;
  }

  /// Updates the reference text and invalidates cached resolution.
  void setRefText(std::string refText) {
    std::scoped_lock lock(_mutex);
    _refText = std::move(refText);
    clearLinkState();
  }

  /// Returns `true` when resolution completed successfully.
  [[nodiscard]] bool isResolved() const noexcept {
    return _state.load(std::memory_order_acquire) == ReferenceState::Resolved;
  }

  /// Returns `true` when resolution completed with an error.
  [[nodiscard]] bool hasError() const noexcept {
    return _state.load(std::memory_order_acquire) == ReferenceState::Error;
  }

  /// Returns the current lazy-resolution state.
  [[nodiscard]] ReferenceState state() const noexcept {
    return _state.load(std::memory_order_acquire);
  }

  /// Returns the document id of the first resolved target, if any.
  [[nodiscard]] workspace::DocumentId getTargetDocumentId() const noexcept {
    if (_targets.empty() || _targets.front().description == nullptr) {
      return workspace::InvalidDocumentId;
    }
    return _targets.front().description->documentId;
  }

  /// Returns the last resolution error message, if any.
  [[nodiscard]] const std::string &getErrorMessage() const noexcept {
    return _errorMessage;
  }

  /// Marks the reference as currently resolving.
  ///
  /// Generic linkers use this to detect cycles during lazy resolution.
  void markResolving() const {
    std::scoped_lock lock(_mutex);
    _targets.clear();
    _errorMessage.clear();
    _state.store(ReferenceState::Resolving, std::memory_order_release);
  }

  /// Drops cached targets and returns the reference to the unresolved state.
  void clearLinkState() const noexcept {
    _state.store(ReferenceState::Unresolved, std::memory_order_release);
    _targets.clear();
    _errorMessage.clear();
  }

  /// Stores the resolution result for a single-valued reference.
  void setResolution(const ReferenceResolution &resolution) const {
    std::scoped_lock lock(_mutex);
    _targets.clear();
    if (resolution.node != nullptr || resolution.description != nullptr) {
      _targets.push_back(
          Target{.node = resolution.node, .description = resolution.description});
      _errorMessage.clear();
      _state.store(ReferenceState::Resolved, std::memory_order_release);
      return;
    }
    _errorMessage = resolution.errorMessage;
    _state.store(ReferenceState::Error, std::memory_order_release);
  }

  /// Stores the resolution result for a multi-valued reference.
  void setResolution(const MultiReferenceResolution &resolution) const {
    std::scoped_lock lock(_mutex);
    _targets.clear();
    _targets.reserve(resolution.items.size());
    for (const auto &item : resolution.items) {
      if (item.node == nullptr && item.description == nullptr) {
        continue;
      }
      _targets.push_back(
          Target{.node = item.node, .description = item.description});
    }
    if (_targets.empty()) {
      _errorMessage = resolution.errorMessage;
      _state.store(ReferenceState::Error, std::memory_order_release);
      return;
    }
    _errorMessage.clear();
    _state.store(ReferenceState::Resolved, std::memory_order_release);
  }

  /// Resolves and returns the first target node, or `nullptr` on failure.
  [[nodiscard]] AstNode *resolve() const {
    ensureResolved();
    if (_targets.empty()) {
      return nullptr;
    }
    return _targets.front().node;
  }

  /// Resolves and returns every target node compatible with the linker result.
  [[nodiscard]] std::vector<AstNode *> resolveAll() const {
    ensureResolved();
    std::vector<AstNode *> nodes;
    nodes.reserve(_targets.size());
    for (const auto &target : _targets) {
      if (target.node != nullptr) {
        nodes.push_back(target.node);
      }
    }
    return nodes;
  }

  /// Visits every resolved target description after ensuring resolution.
  template <typename Fn>
  void forEachResolvedTargetDescription(Fn &&fn) const {
    ensureResolved();
    for (const auto &target : _targets) {
      if (target.description != nullptr) {
        std::forward<Fn>(fn)(target.description);
      }
    }
  }

  /// Initializes generic metadata for a concrete reference type.
  template <typename T>
  void initialize(AstNode *container, std::string refText,
                  std::optional<CstNodeView> refNode, bool multi,
                  const references::Linker *linker = nullptr,
                  std::string_view property = {},
                  std::optional<std::size_t> propertyIndex =
                      std::nullopt) noexcept {
    _container = container;
    _referenceType = std::type_index(typeid(T));
    _accepts = &acceptsImpl<T>;
    _property = property;
    _propertyIndex = propertyIndex.value_or(kNoPropertyIndex);
    _refText = std::move(refText);
    _refNode = refNode.value_or(CstNodeView{});
    _linker = linker;
    _isMulti = multi;
    clearLinkState();
  }

protected:
  struct Target {
    AstNode *node = nullptr;
    const workspace::AstNodeDescription *description = nullptr;
  };

protected:
  static constexpr std::size_t kNoPropertyIndex =
      std::numeric_limits<std::size_t>::max();

  template <typename T>
  static bool acceptsImpl(const AstNode *node) noexcept {
    return dynamic_cast<const T *>(node) != nullptr;
  }

  AstNode *_container = nullptr;
  std::type_index _referenceType = std::type_index(typeid(void));
  bool (*_accepts)(const AstNode *) = nullptr;
  std::string_view _property;
  std::size_t _propertyIndex = kNoPropertyIndex;
  std::string _refText;
  CstNodeView _refNode;
  const references::Linker *_linker = nullptr;
  bool _isMulti = false;
  mutable std::atomic<ReferenceState> _state = ReferenceState::Unresolved;
  mutable std::vector<Target> _targets;
  mutable std::string _errorMessage;
  mutable std::recursive_mutex _mutex;

  void ensureResolved() const;
};

/// Builds a `ReferenceInfo` view from one concrete reference object.
[[nodiscard]] inline ReferenceInfo
makeReferenceInfo(const AbstractReference &reference) noexcept {
  return ReferenceInfo{
      .reference = &reference,
      .container = reference.getContainer(),
      .property = reference.getProperty(),
      .index = reference.getPropertyIndex(),
  };
}

/// Typed single-valued AST reference.
template <typename T> struct Reference : AbstractReference {
  Reference() noexcept : AbstractReference(false) {
    initialize<T>(nullptr, {}, std::nullopt, false);
  }

  /// Creates an unresolved reference from textual source only.
  explicit Reference(std::string refText) noexcept : Reference() {
    initialize<T>(nullptr, std::move(refText), std::nullopt, false);
  }

  /// Creates an unresolved reference from source text and CST location.
  Reference(std::string refText, const CstNodeView &refNode) noexcept
      : Reference() {
    initialize<T>(nullptr, std::move(refText), refNode, false);
  }

  /// Resolves and returns the target casted to `T`.
  [[nodiscard]] T *get() const {
    auto *node = resolve();
    if (node == nullptr) {
      return nullptr;
    }
    if (auto *casted = dynamic_cast<T *>(node); casted != nullptr) {
      return casted;
    }
    std::scoped_lock lock(_mutex);
    _targets.clear();
    _errorMessage =
        "Incompatible reference target type: resolved node cannot be cast "
        "to the expected reference type.";
    _state.store(ReferenceState::Error, std::memory_order_release);
    return nullptr;
  }

  /// Convenience access to the resolved target.
  ///
  /// The pointer is asserted to be non-null in debug builds.
  T *operator->() {
    auto *ptr = get();
    assert(ptr != nullptr);
    return ptr;
  }

  /// Const convenience access to the resolved target.
  const T *operator->() const {
    const auto *ptr = get();
    assert(ptr != nullptr);
    return ptr;
  }

  /// Returns `true` when the reference currently resolves to a compatible target.
  explicit operator bool() const { return get() != nullptr; }
};

/// One typed entry returned by `MultiReference<T>::items()`.
template <typename T> struct MultiReferenceItem {
  T *ref = nullptr;
  const workspace::AstNodeDescription *description = nullptr;
};

/// Typed multi-valued AST reference.
template <typename T> struct MultiReference : AbstractReference {
  MultiReference() noexcept : AbstractReference(true) {
    initialize<T>(nullptr, {}, std::nullopt, true);
  }

  /// Creates an unresolved multi-reference from textual source only.
  explicit MultiReference(std::string refText) noexcept : MultiReference() {
    initialize<T>(nullptr, std::move(refText), std::nullopt, true);
  }

  /// Creates an unresolved multi-reference from source text and CST location.
  MultiReference(std::string refText, const CstNodeView &refNode) noexcept
      : MultiReference() {
    initialize<T>(nullptr, std::move(refText), refNode, true);
  }

  /// Resolves and returns every target casted to `T`.
  [[nodiscard]] std::vector<T *> get() const {
    std::vector<T *> result;
    for (auto *node : resolveAll()) {
      if (auto *casted = dynamic_cast<T *>(node); casted != nullptr) {
        result.push_back(casted);
      }
    }
    return result;
  }

  /// Resolves and returns typed targets together with their descriptions.
  [[nodiscard]] std::vector<MultiReferenceItem<T>> items() const {
    (void)resolveAll();
    std::vector<MultiReferenceItem<T>> result;
    result.reserve(_targets.size());
    for (const auto &target : _targets) {
      if (auto *casted = dynamic_cast<T *>(target.node); casted != nullptr) {
        result.push_back(MultiReferenceItem<T>{.ref = casted,
                                               .description = target.description});
      }
    }
    return result;
  }

  /// Returns the number of typed resolved targets.
  [[nodiscard]] std::size_t size() const { return items().size(); }

  /// Returns `true` when no typed target is currently resolved.
  [[nodiscard]] bool empty() const { return items().empty(); }

  /// Returns `true` when at least one typed target is currently resolved.
  explicit operator bool() const { return !empty(); }
};

/// Type trait detecting pegium reference types.
template <typename T> struct is_reference : std::false_type {};
template <typename T> struct is_reference<Reference<T>> : std::true_type {};
template <typename T> struct is_reference<MultiReference<T>> : std::true_type {};
template <typename T>
struct is_reference<std::vector<Reference<T>>> : std::true_type {};
template <typename T> constexpr bool is_reference_v = is_reference<T>::value;

} // namespace pegium
