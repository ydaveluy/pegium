#pragma once

#include <atomic>
#include <cassert>
#include <string>
#include <string_view>
#include <thread>
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

/// Error subtypes are kept contiguous and last so `hasError()` collapses to a
/// single `state >= kFirstErrorState` comparison.
enum class ReferenceState : std::uint8_t {
  Unresolved,
  Resolving,
  Resolved,
  ErrorNoLinker,
  ErrorNotFound,
  ErrorCycle,
  ErrorException,
};

inline constexpr ReferenceState kFirstErrorState =
    ReferenceState::ErrorNoLinker;

class AbstractReference {
public:
  virtual ~AbstractReference() noexcept = default;

  AbstractReference(const AbstractReference &) = delete;
  AbstractReference &operator=(const AbstractReference &) = delete;

  /// Move construction copies the current state snapshot. Only safe when no
  /// other thread holds the resolver role on `other` (true during AST build).
  AbstractReference(AbstractReference &&other) noexcept
      : _container(other._container), _assignment(other._assignment),
        _refText(std::move(other._refText)), _refNode(other._refNode),
        _linker(other._linker),
        _state(other._state.load(std::memory_order_acquire)),
        _resolvingOwner(other._resolvingOwner.load(std::memory_order_acquire)),
        _isMulti(other._isMulti) {}

  AbstractReference &operator=(AbstractReference &&other) noexcept {
    if (this != &other) {
      _container = other._container;
      _assignment = other._assignment;
      _refText = std::move(other._refText);
      _refNode = other._refNode;
      _linker = other._linker;
      _state.store(other._state.load(std::memory_order_acquire),
                   std::memory_order_release);
      _resolvingOwner.store(
          other._resolvingOwner.load(std::memory_order_acquire),
          std::memory_order_release);
      _waiterCount.store(0, std::memory_order_release);
      _isMulti = other._isMulti;
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

  /// Returns the originating CST node, or an invalid view when none is set.
  ///
  /// Callers should check `view.valid()` before using the result.
  [[nodiscard]] CstNodeView getRefNode() const noexcept { return _refNode; }

  void setRefNode(const CstNodeView &node) noexcept {
    _refNode = node;
    clearLinkState();
  }

  [[nodiscard]] const std::string &getRefText() const noexcept {
    return _refText;
  }

  void setRefText(std::string refText) {
    _refText = std::move(refText);
    clearLinkState();
  }

  [[nodiscard]] bool isResolved() const noexcept {
    return _state.load(std::memory_order_acquire) == ReferenceState::Resolved;
  }

  [[nodiscard]] bool hasError() const noexcept {
    return _state.load(std::memory_order_acquire) >= kFirstErrorState;
  }

  [[nodiscard]] ReferenceState state() const noexcept {
    return _state.load(std::memory_order_acquire);
  }

  /// Returns an empty string when the reference is not in an error state.
  [[nodiscard]] std::string getErrorMessage() const;

  virtual void clearLinkState() const noexcept = 0;

  /// Forces resolution to run for its side effects (populating the cached
  /// result or error state) and discards the resolved value. Used by the
  /// linker's warm-up pass to resolve every reference up front.
  virtual void forceResolve() const = 0;

  [[nodiscard]] bool isMultiReference() const noexcept { return _isMulti; }

  void initialize(AstNode &container, std::string refText,
                  CstNodeView refNode,
                  const grammar::Assignment &assignment,
                  const references::Linker &linker) noexcept {
    _container = std::addressof(container);
    _assignment = std::addressof(assignment);
    _refText = std::move(refText);
    _refNode = refNode;
    _linker = std::addressof(linker);
    clearLinkState();
  }

protected:
  explicit AbstractReference(bool isMulti) noexcept : _isMulti(isMulti) {}

  /// Resets the cached resolution. Caller must guarantee no concurrent reader
  /// (typically invoked by the linker during a workspace write).
  void resetBaseLinkState() const noexcept {
    _resolvingOwner.store(std::thread::id{}, std::memory_order_release);
    _state.store(ReferenceState::Unresolved, std::memory_order_release);
  }

  /// Publishes a terminal state and wakes up any thread waiting on the
  /// resolver. Must only be called by the thread that holds the resolver role
  /// (i.e. the one that won `acquireResolverRole`). Skips `notify_all` when no
  /// thread is currently parked on the state, which is the common case during
  /// single-threaded builds.
  void publishState(ReferenceState newState) const noexcept {
    _resolvingOwner.store(std::thread::id{}, std::memory_order_release);
    _state.store(newState, std::memory_order_release);
    if (_waiterCount.load(std::memory_order_acquire) > 0) {
      _state.notify_all();
    }
  }

  /// Maps a `LinkingErrorKind` returned by the linker into the matching
  /// reference state and publishes it. `Retryable` becomes `Unresolved` so the
  /// next access re-attempts resolution.
  void applyLinkingError(workspace::LinkingErrorKind kind) const noexcept {
    using enum workspace::LinkingErrorKind;
    ReferenceState newState = ReferenceState::ErrorException;
    switch (kind) {
    case Retryable:
      newState = ReferenceState::Unresolved;
      break;
    case Cycle:
      newState = ReferenceState::ErrorCycle;
      break;
    case Exception:
      newState = ReferenceState::ErrorException;
      break;
    case NotFound:
      newState = ReferenceState::ErrorNotFound;
      break;
    }
    publishState(newState);
  }

  /// Tries to acquire the resolver role for this reference.
  ///
  /// Returns `true` when the calling thread successfully transitioned the
  /// state from `Unresolved` to `Resolving` and must now perform the
  /// resolution work. Returns `false` when the state is already terminal
  /// (`Resolved` or one of the `Error*` states), in which case the caller
  /// must read the published value without further work.
  ///
  /// If another thread is already resolving, this call blocks via
  /// `std::atomic::wait` until that thread publishes a terminal state.
  ///
  /// Throws `CyclicReferenceResolution` when the same thread re-enters this
  /// method while still owning the resolver role for the same reference,
  /// indicating that resolving the target recursively requires the target
  /// itself.
  bool acquireResolverRole() const {
    using enum ReferenceState;
    auto state = _state.load(std::memory_order_acquire);
    while (true) {
      if (state == Resolved || state >= kFirstErrorState) {
        return false;
      }
      if (state == Resolving) {
        if (_resolvingOwner.load(std::memory_order_acquire) ==
            std::this_thread::get_id()) {
          throw CyclicReferenceResolution(*this);
        }
        _waiterCount.fetch_add(1, std::memory_order_acq_rel);
        _state.wait(state, std::memory_order_acquire);
        _waiterCount.fetch_sub(1, std::memory_order_acq_rel);
        state = _state.load(std::memory_order_acquire);
        continue;
      }
      if (_state.compare_exchange_weak(state, Resolving,
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
        _resolvingOwner.store(std::this_thread::get_id(),
                              std::memory_order_release);
        return true;
      }
    }
  }

  /// Runs the one-shot resolution protocol shared by every concrete reference,
  /// keeping the resolver-liveness invariant in a single place. Acquires the
  /// resolver role (returning early when another thread already published a
  /// terminal state), guards against a missing linker, then runs `doResolve`,
  /// which performs the linker call and publishes either an error (via
  /// `applyLinkingError`) or `Resolved`. A cyclic resolution publishes
  /// `ErrorCycle`; any other exception runs `onFail` (caller-specific cleanup),
  /// publishes `ErrorException` so waiters never strand at `Resolving`, then
  /// rethrows.
  template <typename DoResolve, typename OnFail>
  void runResolution(DoResolve &&doResolve, OnFail &&onFail) const {
    if (!acquireResolverRole()) {
      return;
    }
    if (_linker == nullptr) {
      publishState(ReferenceState::ErrorNoLinker);
      return;
    }
    try {
      doResolve();
    } catch (const CyclicReferenceResolution &) {
      applyLinkingError(workspace::LinkingErrorKind::Cycle);
    } catch (...) {
      onFail();
      publishState(ReferenceState::ErrorException);
      throw;
    }
  }

  AstNode *_container = nullptr;
  const grammar::Assignment *_assignment = nullptr;
  std::string _refText;
  CstNodeView _refNode;
  const references::Linker *_linker = nullptr;
  mutable std::atomic<ReferenceState> _state{ReferenceState::Unresolved};
  mutable std::atomic<std::thread::id> _resolvingOwner{};
  mutable std::atomic<unsigned> _waiterCount{0};
  bool _isMulti;
};

class AbstractSingleReference : public AbstractReference {
public:
  AbstractSingleReference() noexcept : AbstractReference(false) {}
  [[nodiscard]] virtual const AstNode *resolve() const = 0;
  /// Returns the resolved target description.
  ///
  /// Callers must only use this after the reference reached `Resolved`.
  [[nodiscard]] virtual const workspace::AstNodeDescription &
  resolvedDescription() const = 0;
};

class AbstractMultiReference : public AbstractReference {
public:
  AbstractMultiReference() noexcept : AbstractReference(true) {}

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
