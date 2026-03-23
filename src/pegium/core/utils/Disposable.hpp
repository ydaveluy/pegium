#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace pegium::utils {

/// Interface for objects that release external resources on demand.
class Disposable {
public:
  virtual ~Disposable() noexcept = default;
  virtual void dispose() = 0;
};

/// RAII wrapper around one disposal callback.
class ScopedDisposable final : public Disposable {
public:
  ScopedDisposable() = default;
  explicit ScopedDisposable(std::function<void()> callback);

  ScopedDisposable(ScopedDisposable &&other) noexcept;
  ScopedDisposable &operator=(ScopedDisposable &&other) noexcept;

  ScopedDisposable(const ScopedDisposable &) = delete;
  ScopedDisposable &operator=(const ScopedDisposable &) = delete;

  ~ScopedDisposable() noexcept override;

  void dispose() override;
  [[nodiscard]] bool disposed() const noexcept { return _disposed; }

private:
  std::function<void()> _callback;
  bool _disposed = false;
};

/// Aggregates multiple disposables and disposes them together.
class DisposableStore final : public Disposable {
public:
  void add(ScopedDisposable disposable);
  void dispose() override;
  [[nodiscard]] bool disposed() const noexcept;

private:
  mutable std::mutex _mutex;
  std::vector<ScopedDisposable> _disposables;
  bool _disposed = false;
};

} // namespace pegium::utils
