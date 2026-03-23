#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>

#include <pegium/core/references/Linker.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>
#include <pegium/core/syntax-tree/ReferenceInfo.hpp>
#include <pegium/core/workspace/Symbol.hpp>

namespace pegium {
namespace grammar {
struct Assignment;
}

struct AstNode;

class AbstractReference;
class AbstractSingleReference;
class AbstractMultiReference;

class CyclicReferenceResolution final : public std::exception {
public:
  explicit CyclicReferenceResolution(const AbstractReference &reference) noexcept
      : _reference(&reference) {}

  [[nodiscard]] const AbstractReference &reference() const noexcept {
    assert(_reference != nullptr);
    return *_reference;
  }

  [[nodiscard]] const char *what() const noexcept override {
    return "Cyclic reference resolution detected.";
  }

private:
  const AbstractReference *_reference = nullptr;
};

/// Non-owning handle to a concrete reference stored inside an AST object.
///
/// `ReferenceHandle` lets generic services enumerate references without knowing
/// whether they are stored directly, inside an optional, or inside a vector.
/// Handles stored in managed parse/document results are always concrete.
struct ReferenceHandle {
  using Getter = AbstractReference *(*)(void *, std::size_t) noexcept;
  using ConstGetter =
      const AbstractReference *(*)(const void *, std::size_t) noexcept;

  void *owner = nullptr;
  Getter getter = nullptr;
  ConstGetter constGetter = nullptr;
  std::size_t index = 0;

  [[nodiscard]] AbstractReference *get() const noexcept {
    assert(owner != nullptr);
    assert(getter != nullptr);
    auto *reference = getter(owner, index);
    assert(reference != nullptr);
    return reference;
  }

  [[nodiscard]] const AbstractReference *getConst() const noexcept {
    assert(owner != nullptr);
    assert(constGetter != nullptr);
    const auto *reference = constGetter(owner, index);
    assert(reference != nullptr);
    return reference;
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return owner != nullptr && getter != nullptr && constGetter != nullptr;
  }

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

  template <typename RefVector>
  static ReferenceHandle indexed(RefVector *references,
                                 std::size_t index) noexcept {
    using Ref = typename RefVector::value_type;
    static_assert(std::derived_from<Ref, AbstractReference>);
    return ReferenceHandle{
        .owner = references,
        .getter = [](void *owner, std::size_t index) noexcept
            -> AbstractReference * {
          auto &items = *static_cast<RefVector *>(owner);
          assert(index < items.size());
          return &items[index];
        },
        .constGetter =
            [](const void *owner,
               std::size_t index) noexcept -> const AbstractReference * {
          const auto &items = *static_cast<const RefVector *>(owner);
          assert(index < items.size());
          return &items[index];
        },
        .index = index,
    };
  }

  template <typename RefOptional>
  static ReferenceHandle optional(RefOptional *reference) noexcept {
    using Ref = typename RefOptional::value_type;
    static_assert(std::derived_from<Ref, AbstractReference>);
    return ReferenceHandle{
        .owner = reference,
        .getter = [](void *owner, std::size_t) noexcept -> AbstractReference * {
          auto &item = *static_cast<RefOptional *>(owner);
          assert(item.has_value());
          return &item.value();
        },
        .constGetter =
            [](const void *owner,
               std::size_t) noexcept -> const AbstractReference * {
          const auto &item = *static_cast<const RefOptional *>(owner);
          assert(item.has_value());
          return &item.value();
        },
        .index = 0,
    };
  }
};

enum class ReferenceState : std::uint8_t {
  Unresolved,
  Resolving,
  Resolved,
  Error,
};

class AbstractReference {
public:
  AbstractReference() = default;
  virtual ~AbstractReference() noexcept = default;

  AbstractReference(const AbstractReference &) = delete;
  AbstractReference &operator=(const AbstractReference &) = delete;

  AbstractReference(AbstractReference &&other) noexcept
      : _container(other._container), _assignment(other._assignment),
        _refText(std::move(other._refText)), _refNode(other._refNode),
        _linker(other._linker), _state(other._state) {}

  AbstractReference &operator=(AbstractReference &&other) noexcept {
    if (this != &other) {
      _container = other._container;
      _assignment = other._assignment;
      _refText = std::move(other._refText);
      _refNode = other._refNode;
      _linker = other._linker;
      _state = other._state;
    }
    return *this;
  }

  [[nodiscard]] AstNode *getContainer() const noexcept { return _container; }
  [[nodiscard]] const grammar::Assignment &getAssignment() const noexcept {
    assert(_assignment != nullptr);
    return *_assignment;
  }
  [[nodiscard]] std::type_index getReferenceType() const noexcept;
  [[nodiscard]] std::string_view getFeature() const noexcept;

  [[nodiscard]] std::optional<CstNodeView> getRefNode() const noexcept {
    if (_refNode.valid()) {
      return _refNode;
    }
    return std::nullopt;
  }

  void setRefNode(const CstNodeView &node) noexcept {
    _refNode = node;
    clearLinkStateUnlocked();
  }

  [[nodiscard]] const std::string &getRefText() const noexcept {
    return _refText;
  }

  void setRefText(std::string refText) {
    _refText = std::move(refText);
    clearLinkStateUnlocked();
  }

  [[nodiscard]] bool isResolved() const noexcept {
    return _state == ReferenceState::Resolved;
  }

  [[nodiscard]] bool hasError() const noexcept {
    return _state == ReferenceState::Error;
  }

  [[nodiscard]] ReferenceState state() const noexcept {
    return _state;
  }

  [[nodiscard]] virtual std::string_view getErrorMessage() const noexcept = 0;

  void clearLinkState() const noexcept {
    clearLinkStateUnlocked();
  }

  [[nodiscard]] virtual bool isMultiReference() const noexcept = 0;

  void initialize(AstNode &container, std::string refText,
                  std::optional<CstNodeView> refNode,
                  const grammar::Assignment &assignment,
                  const references::Linker &linker) noexcept {
    _container = std::addressof(container);
    _assignment = std::addressof(assignment);
    _refText = std::move(refText);
    _refNode = refNode.value_or(CstNodeView{});
    _linker = std::addressof(linker);
    clearLinkStateUnlocked();
  }

protected:
  void resetBaseLinkStateUnlocked() const noexcept {
    _state = ReferenceState::Unresolved;
  }

  virtual void clearLinkStateUnlocked() const noexcept = 0;

  AstNode *_container = nullptr;
  const grammar::Assignment *_assignment = nullptr;
  std::string _refText;
  CstNodeView _refNode;
  const references::Linker *_linker = nullptr;
  mutable ReferenceState _state = ReferenceState::Unresolved;
};

class AbstractSingleReference : public AbstractReference {
public:
  using AbstractReference::AbstractReference;
  [[nodiscard]] bool isMultiReference() const noexcept final { return false; }
  [[nodiscard]] virtual const AstNode *resolve() const = 0;
  /// Returns the resolved target description.
  ///
  /// Callers must only use this after the reference reached `Resolved`.
  [[nodiscard]] virtual const workspace::AstNodeDescription &
  resolvedDescription() const = 0;
};

class AbstractMultiReference : public AbstractReference {
public:
  using AbstractReference::AbstractReference;
  [[nodiscard]] bool isMultiReference() const noexcept final { return true; }

  [[nodiscard]] virtual std::size_t resolvedDescriptionCount() const = 0;
  [[nodiscard]] virtual const workspace::AstNodeDescription &
  resolvedDescriptionAt(std::size_t index) const = 0;
};

[[nodiscard]] inline ReferenceInfo
makeReferenceInfo(const AbstractReference &reference) noexcept {
  return ReferenceInfo{reference.getContainer(), reference.getRefText(),
                       reference.getAssignment()};
}

} // namespace pegium
