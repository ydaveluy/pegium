#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium {
struct AstNode;
}

namespace pegium::workspace {

/// Multimap of local scope entries keyed by their AST container node.
class LocalSymbols {
public:
  using storage_type =
      std::unordered_multimap<const AstNode *, AstNodeDescription>;
  using iterator = storage_type::iterator;
  using const_iterator = storage_type::const_iterator;

  LocalSymbols() = default;

  [[nodiscard]] bool empty() const noexcept { return _symbols.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return _symbols.size(); }
  void clear() noexcept { _symbols.clear(); }

  template <class... Args>
  std::pair<iterator, bool> emplace(Args &&...args) {
    auto it = _symbols.emplace(std::forward<Args>(args)...);
    return {it, true};
  }

  [[nodiscard]] std::pair<iterator, iterator>
  equal_range(const AstNode *node) noexcept {
    return _symbols.equal_range(node);
  }

  [[nodiscard]] std::pair<const_iterator, const_iterator>
  equal_range(const AstNode *node) const noexcept {
    return _symbols.equal_range(node);
  }

  [[nodiscard]] iterator begin() noexcept { return _symbols.begin(); }
  [[nodiscard]] const_iterator begin() const noexcept { return _symbols.begin(); }
  [[nodiscard]] const_iterator cbegin() const noexcept { return _symbols.cbegin(); }
  [[nodiscard]] iterator end() noexcept { return _symbols.end(); }
  [[nodiscard]] const_iterator end() const noexcept { return _symbols.end(); }
  [[nodiscard]] const_iterator cend() const noexcept { return _symbols.cend(); }
private:
  storage_type _symbols;
};

} // namespace pegium::workspace
