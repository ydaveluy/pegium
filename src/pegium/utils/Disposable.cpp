#include <pegium/utils/Disposable.hpp>

#include <utility>

namespace pegium::utils {

ScopedDisposable::ScopedDisposable(std::function<void()> callback)
    : _callback(std::move(callback)) {}

ScopedDisposable::ScopedDisposable(ScopedDisposable &&other) noexcept
    : _callback(std::move(other._callback)),
      _disposed(std::exchange(other._disposed, true)) {}

ScopedDisposable &ScopedDisposable::operator=(
    ScopedDisposable &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  dispose();
  _callback = std::move(other._callback);
  _disposed = std::exchange(other._disposed, true);
  return *this;
}

ScopedDisposable::~ScopedDisposable() noexcept { dispose(); }

void ScopedDisposable::dispose() {
  if (_disposed) {
    return;
  }
  _disposed = true;
  if (_callback) {
    _callback();
  }
}

void DisposableStore::add(ScopedDisposable disposable) {
  std::scoped_lock lock(_mutex);
  if (_disposed) {
    disposable.dispose();
    return;
  }
  _disposables.push_back(std::move(disposable));
}

void DisposableStore::dispose() {
  std::vector<ScopedDisposable> local;
  {
    std::scoped_lock lock(_mutex);
    if (_disposed) {
      return;
    }
    _disposed = true;
    local.swap(_disposables);
  }

  for (auto &disposable : local) {
    disposable.dispose();
  }
}

bool DisposableStore::disposed() const noexcept {
  std::scoped_lock lock(_mutex);
  return _disposed;
}

} // namespace pegium::utils
