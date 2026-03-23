#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/Disposable.hpp>
#include <pegium/core/workspace/DocumentBuilder.hpp>

namespace pegium::utils {

/// Base class for caches that can release their state and subscriptions.
class DisposableCache : public Disposable {
public:
  virtual ~DisposableCache() noexcept = default;

  void onDispose(ScopedDisposable disposable) {
    throwIfDisposed();
    _toDispose.add(std::move(disposable));
  }

  void dispose() override {
    {
      std::scoped_lock lock(_disposeMutex);
      if (_disposed) {
        return;
      }
    }

    clear();

    {
      std::scoped_lock lock(_disposeMutex);
      _disposed = true;
    }
    _toDispose.dispose();
  }

  [[nodiscard]] bool disposed() const noexcept {
    std::scoped_lock lock(_disposeMutex);
    return _disposed;
  }

  virtual void clear() = 0;

protected:
  void throwIfDisposed() const {
    std::scoped_lock lock(_disposeMutex);
    if (_disposed) {
      throw std::runtime_error("This cache has already been disposed.");
    }
  }

private:
  mutable std::mutex _disposeMutex;
  bool _disposed = false;
  DisposableStore _toDispose;
};

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
/// Thread-safe key/value cache.
class SimpleCache : public DisposableCache {
public:
  ~SimpleCache() noexcept override {
    try {
      this->dispose();
    } catch (...) {
    }
  }

  [[nodiscard]] bool has(const K &key) const {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    return _cache.contains(key);
  }

  void set(K key, V value) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    _cache.insert_or_assign(std::move(key), std::move(value));
  }

  [[nodiscard]] std::optional<V> get(const K &key) const {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    const auto it = _cache.find(key);
    if (it == _cache.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  template <typename Provider> V get(const K &key, Provider &&provider) const {
    this->throwIfDisposed();
    {
      std::scoped_lock lock(_mutex);
      const auto it = _cache.find(key);
      if (it != _cache.end()) {
        return it->second;
      }
    }

    auto value = provider();
    {
      std::scoped_lock lock(_mutex);
      const auto [it, inserted] = _cache.try_emplace(key, std::move(value));
      (void)inserted;
      return it->second;
    }
  }

  [[nodiscard]] bool erase(const K &key) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    return _cache.erase(key) > 0;
  }

  void clear() override {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    _cache.clear();
  }

private:
  mutable std::mutex _mutex;
  mutable std::unordered_map<K, V, Hash, KeyEqual> _cache;
};

template <typename Context, typename Key, typename Value,
          typename ContextKey = Context, typename KeyHash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename ContextHash = std::hash<ContextKey>,
          typename ContextKeyEqual = std::equal_to<ContextKey>>
/// Thread-safe key/value cache partitioned by a context key.
class ContextCache : public DisposableCache {
public:
  explicit ContextCache(
      std::function<ContextKey(const Context &)> converter = {})
      : _converter(std::move(converter)) {}

  ~ContextCache() noexcept override {
    try {
      this->dispose();
    } catch (...) {
    }
  }

  [[nodiscard]] bool has(const Context &context, const Key &key) const {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    return cacheForContextLocked(context).contains(key);
  }

  void set(const Context &context, Key key, Value value) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    cacheForContextLocked(context).insert_or_assign(std::move(key),
                                                    std::move(value));
  }

  [[nodiscard]] std::optional<Value> get(const Context &context,
                                         const Key &key) const {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    auto &contextCache = cacheForContextLocked(context);
    const auto it = contextCache.find(key);
    if (it == contextCache.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  template <typename Provider>
  Value get(const Context &context, const Key &key, Provider &&provider) const {
    this->throwIfDisposed();
    {
      std::scoped_lock lock(_mutex);
      auto &contextCache = cacheForContextLocked(context);
      const auto it = contextCache.find(key);
      if (it != contextCache.end()) {
        return it->second;
      }
    }

    auto value = provider();
    {
      std::scoped_lock lock(_mutex);
      auto &contextCache = cacheForContextLocked(context);
      const auto [it, inserted] =
          contextCache.try_emplace(key, std::move(value));
      (void)inserted;
      return it->second;
    }
  }

  [[nodiscard]] bool erase(const Context &context, const Key &key) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    return cacheForContextLocked(context).erase(key) > 0;
  }

  void clear(const Context &context) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    _cache.erase(convert(context));
  }

  void clear() override {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    _cache.clear();
  }

protected:
  [[nodiscard]] ContextKey convert(const Context &context) const {
    if (_converter) {
      return _converter(context);
    }
    return ContextKey(context);
  }

private:
  using ContextMap = std::unordered_map<Key, Value, KeyHash, KeyEqual>;

  ContextMap &cacheForContextLocked(const Context &context) const {
    const auto contextKey = convert(context);
    auto it = _cache.find(contextKey);
    if (it == _cache.end()) {
      it = _cache.try_emplace(contextKey).first;
    }
    return it->second;
  }

  mutable std::mutex _mutex;
  mutable std::unordered_map<ContextKey, ContextMap, ContextHash,
                             ContextKeyEqual>
      _cache;
  std::function<ContextKey(const Context &)> _converter;
};

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
/// Cache automatically invalidated when workspace documents change.
class DocumentCache final : public DisposableCache {
public:
  explicit DocumentCache(const services::SharedCoreServices &sharedServices) {
    auto *documentBuilder = sharedServices.workspace.documentBuilder.get();
    this->onDispose(documentBuilder->onUpdate(
        [this](std::span<const workspace::DocumentId> changedDocumentIds,
               std::span<const workspace::DocumentId> deletedDocumentIds) {
          for (const auto documentId : changedDocumentIds) {
            clear(documentId);
          }
          for (const auto documentId : deletedDocumentIds) {
            clear(documentId);
          }
        }));
  }

  ~DocumentCache() noexcept override {
    try {
      this->dispose();
    } catch (...) {
    }
  }

  [[nodiscard]] bool has(workspace::DocumentId documentId, const K &key) const {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    const auto *cache = findContextCacheLocked(documentId);
    if (cache != nullptr) {
      return cache->values.contains(key);
    }
    return false;
  }

  void set(workspace::DocumentId documentId, K key, V value) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    if (auto *cache = cacheForInsertLocked(documentId); cache != nullptr) {
      cache->values.insert_or_assign(std::move(key), std::move(value));
    }
  }

  [[nodiscard]] std::optional<V> get(workspace::DocumentId documentId,
                                     const K &key) const {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    const auto *cache = findContextCacheLocked(documentId);
    if (cache == nullptr) {
      return std::nullopt;
    }
    const auto it = cache->values.find(key);
    if (it == cache->values.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  template <typename Provider>
  V get(workspace::DocumentId documentId, const K &key,
        Provider &&provider) const {
    this->throwIfDisposed();
    {
      std::scoped_lock lock(_mutex);
      if (const auto *cache = findContextCacheLocked(documentId);
          cache != nullptr) {
        const auto it = cache->values.find(key);
        if (it != cache->values.end()) {
          return it->second;
        }
      }
    }

    auto value = provider();
    {
      std::scoped_lock lock(_mutex);
      auto *cache = cacheForInsertLocked(documentId);
      if (cache == nullptr) {
        return value;
      }
      const auto [it, inserted] =
          cache->values.try_emplace(key, std::move(value));
      (void)inserted;
      return it->second;
    }
  }

  [[nodiscard]] bool erase(workspace::DocumentId documentId, const K &key) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    if (auto *cache = findMutableContextCacheLocked(documentId);
        cache != nullptr) {
      return cache->values.erase(key) > 0;
    }
    return false;
  }

  void clear(workspace::DocumentId documentId) {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    if (documentId != workspace::InvalidDocumentId) {
      _cache.erase(documentId);
    }
  }

  void clear() override {
    this->throwIfDisposed();
    std::scoped_lock lock(_mutex);
    _cache.clear();
  }

private:
  struct DocumentEntry {
    std::unordered_map<K, V, Hash, KeyEqual> values;
  };

  [[nodiscard]] const DocumentEntry *
  findContextCacheLocked(workspace::DocumentId documentId) const {
    if (documentId == workspace::InvalidDocumentId) {
      return nullptr;
    }
    const auto it = _cache.find(documentId);
    return it == _cache.end() ? nullptr : std::addressof(it->second);
  }

  [[nodiscard]] DocumentEntry *
  findMutableContextCacheLocked(workspace::DocumentId documentId) {
    if (documentId == workspace::InvalidDocumentId) {
      return nullptr;
    }
    if (auto it = _cache.find(documentId); it != _cache.end()) {
      return std::addressof(it->second);
    }
    return nullptr;
  }

  [[nodiscard]] DocumentEntry *
  cacheForInsertLocked(workspace::DocumentId documentId) const {
    if (documentId == workspace::InvalidDocumentId) {
      return nullptr;
    }
    auto it = _cache.find(documentId);
    if (it == _cache.end()) {
      it = _cache.try_emplace(documentId).first;
    }
    return std::addressof(it->second);
  }

  mutable std::mutex _mutex;
  mutable std::unordered_map<workspace::DocumentId, DocumentEntry> _cache;
};

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
/// Cache automatically cleared whenever the workspace rebuilds.
class WorkspaceCache final : public SimpleCache<K, V, Hash, KeyEqual> {
public:
  explicit WorkspaceCache(
      const services::SharedCoreServices &sharedServices,
      std::optional<workspace::DocumentState> state = std::nullopt) {
    auto *documentBuilder = sharedServices.workspace.documentBuilder.get();
    if (state.has_value()) {
      this->onDispose(documentBuilder->onBuildPhase(
          *state, [this](std::span<const std::shared_ptr<workspace::Document>>,
                         utils::CancellationToken) { this->clear(); }));
      this->onDispose(documentBuilder->onUpdate(
          [this](std::span<const workspace::DocumentId>,
                 std::span<const workspace::DocumentId> deletedDocumentIds) {
            if (!deletedDocumentIds.empty()) {
              this->clear();
            }
          }));
      return;
    }

    this->onDispose(documentBuilder->onUpdate(
        [this](std::span<const workspace::DocumentId>,
               std::span<const workspace::DocumentId>) { this->clear(); }));
  }
};

} // namespace pegium::utils
