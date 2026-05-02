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
/// The `ScopeProvider` consumes the bucketed view directly via `forContainer`.
/// The per-bucket name index is populated eagerly during `emplace`, so once
/// the build phase completes the structure is fully immutable and safe to
/// query from multiple reader threads concurrently.
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
  /// bound to it. After the build phase completes, this is a pure const
  /// lookup: no mutation, safe under concurrent reads.
  [[nodiscard]] const BucketedScopeEntries *
  forContainer(const AstNode *container) const {
    const auto it = _byContainer.find(container);
    return it == _byContainer.end() ? nullptr : std::addressof(it->second);
  }

  [[nodiscard]] const_iterator begin() const noexcept {
    return _byContainer.begin();
  }
  [[nodiscard]] const_iterator end() const noexcept {
    return _byContainer.end();
  }

private:
  map_type _byContainer;
  std::size_t _totalSize = 0;
};

} // namespace pegium::workspace
