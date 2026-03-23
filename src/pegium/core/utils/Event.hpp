#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <pegium/core/utils/Disposable.hpp>

namespace pegium::utils {

/// Thread-safe event emitter with disposable subscriptions.
template <typename T> class EventEmitter {
public:
  using Listener = std::function<void(const T &)>;

  /// Registers a listener.
  ///
  /// Active listeners are notified in registration order.
  ScopedDisposable on(Listener listener) const {
    const auto state = _state;
    std::size_t id = 0;
    {
      std::scoped_lock lock(state->mutex);
      id = state->nextId++;
      state->listeners.push_back(
          ListenerEntry{.id = id, .listener = std::move(listener)});
    }

    return ScopedDisposable([state, id]() {
      std::scoped_lock lock(state->mutex);
      std::erase_if(state->listeners, [id](const ListenerEntry &entry) {
        return entry.id == id;
      });
    });
  }

  void emit(const T &event) const {
    std::vector<ListenerEntry> listenersCopy;
    {
      std::scoped_lock lock(_state->mutex);
      listenersCopy = _state->listeners;
    }

    for (const auto &entry : listenersCopy) {
      if (entry.listener) {
        entry.listener(event);
      }
    }
  }

  [[nodiscard]] std::size_t listenerCount() const {
    std::scoped_lock lock(_state->mutex);
    return _state->listeners.size();
  }

private:
  struct ListenerEntry {
    std::size_t id = 0;
    Listener listener;
  };

  struct State {
    std::mutex mutex;
    std::vector<ListenerEntry> listeners;
    std::size_t nextId = 0;
  };

  std::shared_ptr<State> _state = std::make_shared<State>();
};

} // namespace pegium::utils
