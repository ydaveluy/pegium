#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory_resource>
#include <utility>
#include <vector>

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/AstReflection.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>

namespace pegium {

namespace workspace {
struct Document;
} // namespace workspace

/// Arena allocator for AstNodes belonging to one parse result.
///
/// Every node created through `create<T>(...)` is allocated from the CST
/// root's monotonic buffer resource, sharing a single pool with the CST.
/// The arena packs nodes in chunks of pointers so that destructors can be
/// invoked in LIFO order at arena teardown without polluting `AstNode` with
/// per-node bookkeeping fields.
///
/// Lifetime: the arena owns every node it produced. Destroying the arena
/// invokes `~AstNode()` (virtual dispatch reaches the most-derived destructor)
/// on every node in reverse construction order. Buffer release is owned by the
/// `RootCstNode`, so the arena MUST be destroyed before its CST root.
class AstArena {
public:
  using NodeId = std::uint32_t;

  /// Constructs an arena bound to `cstRoot`. AST node memory is taken from
  /// the CST root's pool to avoid a second monotonic buffer chain.
  explicit AstArena(RootCstNode &cstRoot) noexcept
      : _pool(cstRoot.memoryResource()), _cstRoot(std::addressof(cstRoot)) {}

  ~AstArena() noexcept { destroyAll(); }

  AstArena(const AstArena &) = delete;
  AstArena &operator=(const AstArena &) = delete;
  AstArena(AstArena &&) = delete;
  AstArena &operator=(AstArena &&) = delete;

  /// Returns the originating CST root.
  [[nodiscard]] RootCstNode *cstRoot() const noexcept { return _cstRoot; }

  /// Attaches the workspace document that owns this arena and the reflection
  /// registry used by `pegium::ast_cast` / `pegium::is_a`.
  void attachDocument(const workspace::Document &document,
                      const AstReflection *reflection = nullptr) noexcept {
    _document = std::addressof(document);
    _reflection = reflection;
  }

  /// Returns the attached workspace document, or nullptr when standalone.
  [[nodiscard]] const workspace::Document *document() const noexcept {
    return _document;
  }

  /// Returns the AST reflection registry attached to this arena, or nullptr
  /// when no language services are bound (e.g. standalone test fixtures).
  [[nodiscard]] const AstReflection *reflection() const noexcept {
    return _reflection;
  }

  /// Allocates and constructs a `T` inside the arena.
  ///
  /// The destructor is automatically invoked at arena teardown.
  template <typename T, typename... Args>
    requires std::derived_from<T, AstNode>
  [[nodiscard]] T *create(Args &&...args) {
    // Reserve the recording slot before constructing the node, so the storeNode
    // write below cannot throw afterwards. A throw here, or later from the pool
    // allocation or the constructor, leaves no recorded node, so a bad_alloc
    // cannot leave a fully-constructed node unrecorded and thus undestroyed at
    // teardown.
    ensureChunkCapacity();
    const NodeId id = _count;
    void *mem = _pool->allocate(sizeof(T), alignof(T));
    auto *obj = ::new (mem) T(std::forward<Args>(args)...);
    obj->_symbolId = id;
    obj->_arena = this;
    storeNode(obj);
    return obj;
  }

  /// Returns the AST node owning the given symbol id, or nullptr if the id is
  /// out of range.
  [[nodiscard]] AstNode *getNode(NodeId id) const noexcept {
    if (id >= _count) {
      return nullptr;
    }
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }

  /// Returns the number of nodes currently owned by this arena.
  [[nodiscard]] NodeId size() const noexcept { return _count; }

  /// Returns whether this arena owns no nodes.
  [[nodiscard]] bool empty() const noexcept { return _count == 0; }

private:
  static constexpr std::uint32_t chunk_size = 1U << 12;
  static constexpr std::uint32_t chunk_shift = std::bit_width(chunk_size) - 1U;
  static constexpr std::uint32_t chunk_mask = chunk_size - 1U;
  static_assert(std::has_single_bit(chunk_size),
                "chunk_size must be power of two");

  // Ensures there is room to record one more node, making the later storeNode
  // write non-throwing. Runs before the node is constructed: a throw here, or
  // later from the pool allocation or the constructor, leaves no recorded node,
  // so no node is ever left constructed-but-undestroyed at teardown.
  void ensureChunkCapacity() {
    if (static_cast<std::size_t>(_count >> chunk_shift) == _chunks.size())
        [[unlikely]] {
      _chunks.push_back(static_cast<AstNode **>(_pool->allocate(
          sizeof(AstNode *) * chunk_size, alignof(AstNode *))));
    }
  }

  // Records an already-constructed node. noexcept: ensureChunkCapacity() has
  // guaranteed the slot exists.
  void storeNode(AstNode *node) noexcept {
    _chunks[_count >> chunk_shift][_count & chunk_mask] = node;
    ++_count;
  }

  void destroyAll() noexcept {
    // LIFO destruction. The CST root owns the underlying pool buffers.
    for (NodeId i = _count; i > 0; --i) {
      const NodeId idx = i - 1U;
      AstNode *node = _chunks[idx >> chunk_shift][idx & chunk_mask];
      assert(node != nullptr);
      node->~AstNode();
    }
    _count = 0;
  }

  std::pmr::memory_resource *_pool;
  std::vector<AstNode **> _chunks;
  NodeId _count = 0;
  RootCstNode *_cstRoot;
  const workspace::Document *_document = nullptr;
  const AstReflection *_reflection = nullptr;
};

} // namespace pegium
