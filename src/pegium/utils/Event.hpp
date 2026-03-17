#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#include <pegium/utils/Disposable.hpp>

namespace pegium::utils {

template <typename T> class EventEmitter {
public:
  using Listener = std::function<void(const T &)>;

  ScopedDisposable on(Listener listener) {
    const auto state = _state;
    std::size_t id = 0;
    {
      std::scoped_lock lock(state->mutex);
      id = state->nextId++;
      state->listeners.emplace(id, std::move(listener));
    }

    return ScopedDisposable([state, id]() {
      std::scoped_lock lock(state->mutex);
      state->listeners.erase(id);
    });
  }

  void emit(const T &event) const {
    std::unordered_map<std::size_t, Listener> listenersCopy;
    {
      std::scoped_lock lock(_state->mutex);
      listenersCopy = _state->listeners;
    }

    for (const auto &[id, listener] : listenersCopy) {
      (void)id;
      if (listener) {
        listener(event);
      }
    }
  }

  [[nodiscard]] std::size_t listenerCount() const {
    std::scoped_lock lock(_state->mutex);
    return _state->listeners.size();
  }

private:
  struct State {
    std::mutex mutex;
    std::unordered_map<std::size_t, Listener> listeners;
    std::size_t nextId = 0;
  };

  std::shared_ptr<State> _state = std::make_shared<State>();
};

} // namespace pegium::utils
