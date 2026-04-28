#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>

#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium {
struct AstNode;
}

namespace pegium::workspace {

/// Local scope entries of a document, bucketed by container AST node and
/// indexed by symbol type and name.
///
/// Descriptions are stored once, in a `BucketedScopeEntries` per container.
/// The `ScopeProvider` consumes the bucketed view directly via
/// `forContainer`, eliminating the per-document rebuild that previously ran
/// on first lookup.
///
/// Copy is disabled because the per-bucket name index holds raw pointers and
/// `string_view`s into the owned descriptions — duplicating the storage would
/// silently leave the indexes pointing at the original deque elements. Move
/// is safe because both the outer `unordered_map` and inner `std::deque`s
/// preserve element addresses on transfer.
class LocalSymbols {
public:
  using map_type =
      std::unordered_map<const AstNode *, BucketedScopeEntries>;
  using const_iterator = map_type::const_iterator;

  LocalSymbols() = default;
  LocalSymbols(const LocalSymbols &) = delete;
  LocalSymbols &operator=(const LocalSymbols &) = delete;
  LocalSymbols(LocalSymbols &&) noexcept = default;
  LocalSymbols &operator=(LocalSymbols &&) noexcept = default;

  /// Adds `description` under `container`, dispatched to the bucket whose
  /// type matches `description.type`. The returned reference is stable for
  /// the lifetime of this `LocalSymbols` instance.
  const AstNodeDescription &emplace(const AstNode *container,
                                    AstNodeDescription description);

  [[nodiscard]] bool empty() const noexcept { return _byContainer.empty(); }
  /// Total number of descriptions held across all containers (not the number
  /// of containers). Tests rely on this entry-count semantic.
  [[nodiscard]] std::size_t size() const noexcept { return _totalSize; }
  void clear() noexcept {
    _byContainer.clear();
    _totalSize = 0;
  }

  /// Returns the bucketed entries for `container`, or nullptr if no symbol is
  /// bound to it. Lazily populates the per-bucket name index on first call,
  /// so collection-time `emplace` only has to push to the deque.
  ///
  /// The lazy build is single-threaded by contract: it runs once per document
  /// when the linker first probes `container`, before any parallel reader can
  /// observe this `BucketedScopeEntries`. May throw `std::bad_alloc` if the
  /// name-index hash map cannot grow.
  [[nodiscard]] const BucketedScopeEntries *
  forContainer(const AstNode *container) const {
    const auto it = _byContainer.find(container);
    if (it == _byContainer.end()) {
      return nullptr;
    }
    if (!it->second.indexed) {
      buildNameIndex(it->second);
    }
    return std::addressof(it->second);
  }

  [[nodiscard]] const_iterator begin() const noexcept {
    return _byContainer.begin();
  }
  [[nodiscard]] const_iterator end() const noexcept {
    return _byContainer.end();
  }

private:
  /// Populates `bucketed.entriesByName` for every bucket. Mutates the cache
  /// state through `mutable` members on a logically-const accessor.
  static void buildNameIndex(const BucketedScopeEntries &bucketed);

  map_type _byContainer;
  std::size_t _totalSize = 0;
};

} // namespace pegium::workspace
