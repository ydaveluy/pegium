#include <pegium/core/workspace/DefaultWorkspaceLock.hpp>

#include <exception>
#include <mutex>
#include <utility>

#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::workspace {

DefaultWorkspaceLock::DefaultWorkspaceLock()
    : _worker([this](std::stop_token stopToken) { run(stopToken); }) {}

DefaultWorkspaceLock::~DefaultWorkspaceLock() {
  {
    std::scoped_lock lock(_queueMutex);
    _stopping.store(true);
    for (auto &entry : _writeQueue) {
      entry.cancellation.request_stop();
    }
  }
  this->cancelWrite();
  // Wake the worker and any reads blocked on the gate so they observe _stopping.
  _queueCv.notify_all();
  {
    std::unique_lock gate(_gateMutex);
    _gateCv.notify_all();
    // Wait out every read that is mid-flight in the gate before returning, so
    // the gate mutex / condition variable are never destroyed under a reader
    // that is still re-acquiring or unlocking them.
    _gateCv.wait(gate, [this]() { return _activeReads == 0; });
  }
}

std::future<void> DefaultWorkspaceLock::write(WriteAction action) {
  if (!action) {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  WriteEntry entry;
  entry.action = std::move(action);
  auto future = entry.completion.get_future();

  {
    std::scoped_lock lock(_queueMutex);
    if (_stopping.load()) {
      entry.completion.set_value();
      return future;
    }
    _previousWriteCancellation.request_stop();
    _previousWriteCancellation = entry.cancellation;
    // Count the pending write before it becomes poppable, so its eventual
    // matching decrement in downgrade() can never run first (which would
    // underflow the counter and let a read slip past a queued write).
    {
      std::scoped_lock gate(_gateMutex);
      ++_writePending;
    }
    _writeQueue.push_back(std::move(entry));
  }
  _queueCv.notify_all();
  _gateCv.notify_all(); // give the queued write priority over waiting reads
  return future;
}

std::future<void> DefaultWorkspaceLock::read(ReadAction action) {
  std::promise<void> promise;
  auto future = promise.get_future();
  if (!action) {
    promise.set_value();
    return future;
  }

  {
    std::unique_lock gate(_gateMutex);
    ++_activeReads;
    _gateCv.wait(gate, [this]() {
      return _stopping.load() || (!_writeHeld && _writePending == 0);
    });
    if (_stopping.load()) {
      if (--_activeReads == 0) {
        _gateCv.notify_all();
      }
      promise.set_value();
      return future;
    }
    ++_readers;
  }

  try {
    action();
    promise.set_value();
  } catch (...) {
    promise.set_exception(std::current_exception());
  }

  {
    std::scoped_lock gate(_gateMutex);
    --_readers;
    if (--_activeReads == 0 || _readers == 0) {
      _gateCv.notify_all();
    }
  }
  return future;
}

void DefaultWorkspaceLock::cancelWrite() {
  std::scoped_lock lock(_queueMutex);
  _previousWriteCancellation.request_stop();
}

void DefaultWorkspaceLock::run(std::stop_token stopToken) {
  while (true) {
    WriteEntry entry;
    {
      std::unique_lock lock(_queueMutex);
      _queueCv.wait(lock, [this, &stopToken]() {
        return _stopping.load() || stopToken.stop_requested() ||
               !_writeQueue.empty();
      });

      if ((_stopping.load() || stopToken.stop_requested()) &&
          _writeQueue.empty()) {
        return;
      }

      entry = std::move(_writeQueue.front());
      _writeQueue.pop_front();
    }

    const bool shuttingDown = _stopping.load() || stopToken.stop_requested();
    if (shuttingDown) {
      entry.cancellation.request_stop();
    }

    // Acquire exclusivity: wait until no readers and no held write.
    {
      std::unique_lock gate(_gateMutex);
      _gateCv.wait(gate, [this]() {
        return _stopping.load() || (!_writeHeld && _readers == 0);
      });
      _writeHeld = true;
    }

    // Downgrade flips the held write to a reader under one lock, so the write's
    // remaining work runs as a reader and pending/new reads can proceed.
    std::once_flag downgraded;
    auto downgrade = [this, &downgraded]() {
      std::call_once(downgraded, [this]() {
        std::scoped_lock gate(_gateMutex);
        _writeHeld = false;
        ++_readers;
        --_writePending;
        _gateCv.notify_all();
      });
    };

    try {
      if (!shuttingDown) {
        entry.action(entry.cancellation.get_token(), downgrade);
      }
      downgrade(); // safety net: always release exclusivity
      entry.completion.set_value();
    } catch (const utils::OperationCancelled &) {
      downgrade();
      entry.completion.set_value();
    } catch (...) {
      // Catch everything: an escaping non-std exception would kill the sole
      // worker thread, leaving every future write/read queued forever.
      downgrade();
      entry.completion.set_exception(std::current_exception());
    }

    // Release the read lock the downgrade acquired.
    {
      std::scoped_lock gate(_gateMutex);
      if (--_readers == 0) {
        _gateCv.notify_all();
      }
    }
  }
}

} // namespace pegium::workspace
